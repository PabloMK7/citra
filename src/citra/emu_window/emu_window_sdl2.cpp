// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <string>
#define SDL_MAIN_HANDLED
#include <SDL.h>
#include <glad/glad.h>
#include "citra/emu_window/emu_window_sdl2.h"
#include "common/key_map.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/string_util.h"
#include "core/hle/service/hid/hid.h"
#include "core/settings.h"
#include "video_core/video_core.h"

void EmuWindow_SDL2::OnMouseMotion(s32 x, s32 y) {
    TouchMoved((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
}

void EmuWindow_SDL2::OnMouseButton(u32 button, u8 state, s32 x, s32 y) {
    if (button != SDL_BUTTON_LEFT)
        return;

    if (state == SDL_PRESSED) {
        TouchPressed((unsigned)std::max(x, 0), (unsigned)std::max(y, 0));
    } else {
        TouchReleased();
    }
}

void EmuWindow_SDL2::OnKeyEvent(int key, u8 state) {
    if (state == SDL_PRESSED) {
        KeyMap::PressKey(*this, {key, keyboard_id});
    } else if (state == SDL_RELEASED) {
        KeyMap::ReleaseKey(*this, {key, keyboard_id});
    }
}

bool EmuWindow_SDL2::IsOpen() const {
    return is_open;
}

void EmuWindow_SDL2::OnResize() {
    int width, height;

    SDL_GetWindowSize(render_window, &width, &height);

    NotifyFramebufferLayoutChanged(
        EmuWindow::FramebufferLayout::DefaultScreenLayout(width, height));
}

EmuWindow_SDL2::EmuWindow_SDL2() {
    keyboard_id = KeyMap::NewDeviceId();

    ReloadSetKeymaps();

    SDL_SetMainReady();

    // Initialize the window
    if (SDL_Init(SDL_INIT_VIDEO) < 0) {
        LOG_CRITICAL(Frontend, "Failed to initialize SDL2! Exiting...");
        exit(1);
    }

    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, 8);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, 0);

    std::string window_title =
        Common::StringFromFormat("Citra | %s-%s", Common::g_scm_branch, Common::g_scm_desc);
    render_window = SDL_CreateWindow(
        window_title.c_str(),
        SDL_WINDOWPOS_UNDEFINED, // x position
        SDL_WINDOWPOS_UNDEFINED, // y position
        VideoCore::kScreenTopWidth, VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight,
        SDL_WINDOW_OPENGL | SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);

    if (render_window == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 window! Exiting...");
        exit(1);
    }

    gl_context = SDL_GL_CreateContext(render_window);

    if (gl_context == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create SDL2 GL context! Exiting...");
        exit(1);
    }

    if (!gladLoadGLLoader(static_cast<GLADloadproc>(SDL_GL_GetProcAddress))) {
        LOG_CRITICAL(Frontend, "Failed to initialize GL functions! Exiting...");
        exit(1);
    }

    OnResize();
    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);
    SDL_PumpEvents();
    SDL_GL_SetSwapInterval(Settings::values.use_vsync);

    DoneCurrent();
}

EmuWindow_SDL2::~EmuWindow_SDL2() {
    SDL_GL_DeleteContext(gl_context);
    SDL_Quit();
}

void EmuWindow_SDL2::SwapBuffers() {
    SDL_GL_SwapWindow(render_window);
}

void EmuWindow_SDL2::PollEvents() {
    SDL_Event event;

    // SDL_PollEvent returns 0 when there are no more events in the event queue
    while (SDL_PollEvent(&event)) {
        switch (event.type) {
        case SDL_WINDOWEVENT:
            switch (event.window.event) {
            case SDL_WINDOWEVENT_SIZE_CHANGED:
            case SDL_WINDOWEVENT_RESIZED:
            case SDL_WINDOWEVENT_MAXIMIZED:
            case SDL_WINDOWEVENT_RESTORED:
            case SDL_WINDOWEVENT_MINIMIZED:
                OnResize();
                break;
            case SDL_WINDOWEVENT_CLOSE:
                is_open = false;
                break;
            }
            break;
        case SDL_KEYDOWN:
        case SDL_KEYUP:
            OnKeyEvent(static_cast<int>(event.key.keysym.scancode), event.key.state);
            break;
        case SDL_MOUSEMOTION:
            OnMouseMotion(event.motion.x, event.motion.y);
            break;
        case SDL_MOUSEBUTTONDOWN:
        case SDL_MOUSEBUTTONUP:
            OnMouseButton(event.button.button, event.button.state, event.button.x, event.button.y);
            break;
        case SDL_QUIT:
            is_open = false;
            break;
        }
    }
}

void EmuWindow_SDL2::MakeCurrent() {
    SDL_GL_MakeCurrent(render_window, gl_context);
}

void EmuWindow_SDL2::DoneCurrent() {
    SDL_GL_MakeCurrent(render_window, nullptr);
}

void EmuWindow_SDL2::ReloadSetKeymaps() {
    KeyMap::ClearKeyMapping(keyboard_id);
    for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
        KeyMap::SetKeyMapping(
            {Settings::values.input_mappings[Settings::NativeInput::All[i]], keyboard_id},
            KeyMap::mapping_targets[i]);
    }
}

void EmuWindow_SDL2::OnMinimalClientAreaChangeRequest(
    const std::pair<unsigned, unsigned>& minimal_size) {

    SDL_SetWindowMinimumSize(render_window, minimal_size.first, minimal_size.second);
}
