// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdlib>
#include <string>

// Let’s use our own GL header, instead of one from GLFW.
#include "video_core/renderer_opengl/generated/gl_3_2_core.h"
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include "common/assert.h"
#include "common/key_map.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/string_util.h"

#include "video_core/video_core.h"

#include "core/settings.h"
#include "core/hle/service/hid/hid.h"

#include "citra/emu_window/emu_window_glfw.h"

EmuWindow_GLFW* EmuWindow_GLFW::GetEmuWindow(GLFWwindow* win) {
    return static_cast<EmuWindow_GLFW*>(glfwGetWindowUserPointer(win));
}

void EmuWindow_GLFW::OnMouseButtonEvent(GLFWwindow* win, int button, int action, int mods) {
    if (button == GLFW_MOUSE_BUTTON_LEFT) {
        auto emu_window = GetEmuWindow(win);
        auto layout = emu_window->GetFramebufferLayout();
        double x, y;
        glfwGetCursorPos(win, &x, &y);

        if (action == GLFW_PRESS)
            emu_window->TouchPressed(static_cast<unsigned>(x), static_cast<unsigned>(y));
        else if (action == GLFW_RELEASE)
            emu_window->TouchReleased();
    }
}

void EmuWindow_GLFW::OnCursorPosEvent(GLFWwindow* win, double x, double y) {
    GetEmuWindow(win)->TouchMoved(static_cast<unsigned>(std::max(x, 0.0)), static_cast<unsigned>(std::max(y, 0.0)));
}

/// Called by GLFW when a key event occurs
void EmuWindow_GLFW::OnKeyEvent(GLFWwindow* win, int key, int scancode, int action, int mods) {
    auto emu_window = GetEmuWindow(win);
    int keyboard_id = emu_window->keyboard_id;

    if (action == GLFW_PRESS) {
        emu_window->KeyPressed({key, keyboard_id});
    } else if (action == GLFW_RELEASE) {
        emu_window->KeyReleased({key, keyboard_id});
    }
}

/// Whether the window is still open, and a close request hasn't yet been sent
const bool EmuWindow_GLFW::IsOpen() {
    return glfwWindowShouldClose(m_render_window) == 0;
}

void EmuWindow_GLFW::OnFramebufferResizeEvent(GLFWwindow* win, int width, int height) {
    GetEmuWindow(win)->NotifyFramebufferLayoutChanged(EmuWindow::FramebufferLayout::DefaultScreenLayout(width, height));
}

void EmuWindow_GLFW::OnClientAreaResizeEvent(GLFWwindow* win, int width, int height) {
    // NOTE: GLFW provides no proper way to set a minimal window size.
    //       Hence, we just ignore the corresponding EmuWindow hint.
    OnFramebufferResizeEvent(win, width, height);
}

/// EmuWindow_GLFW constructor
EmuWindow_GLFW::EmuWindow_GLFW() {
    keyboard_id = KeyMap::NewDeviceId();

    ReloadSetKeymaps();

    glfwSetErrorCallback([](int error, const char *desc){
        LOG_ERROR(Frontend, "GLFW 0x%08x: %s", error, desc);
    });

    // Initialize the window
    if(glfwInit() != GL_TRUE) {
        LOG_CRITICAL(Frontend, "Failed to initialize GLFW! Exiting...");
        exit(1);
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    // GLFW on OSX requires these window hints to be set to create a 3.2+ GL context.
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    std::string window_title = Common::StringFromFormat("Citra | %s-%s", Common::g_scm_branch, Common::g_scm_desc);
    m_render_window = glfwCreateWindow(VideoCore::kScreenTopWidth,
        (VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight),
        window_title.c_str(), nullptr, nullptr);

    if (m_render_window == nullptr) {
        LOG_CRITICAL(Frontend, "Failed to create GLFW window! Exiting...");
        exit(1);
    }

    glfwSetWindowUserPointer(m_render_window, this);

    // Notify base interface about window state
    int width, height;
    glfwGetFramebufferSize(m_render_window, &width, &height);
    OnFramebufferResizeEvent(m_render_window, width, height);

    glfwGetWindowSize(m_render_window, &width, &height);
    OnClientAreaResizeEvent(m_render_window, width, height);

    // Setup callbacks
    glfwSetKeyCallback(m_render_window, OnKeyEvent);
    glfwSetMouseButtonCallback(m_render_window, OnMouseButtonEvent);
    glfwSetCursorPosCallback(m_render_window, OnCursorPosEvent);
    glfwSetFramebufferSizeCallback(m_render_window, OnFramebufferResizeEvent);
    glfwSetWindowSizeCallback(m_render_window, OnClientAreaResizeEvent);

    DoneCurrent();
}

/// EmuWindow_GLFW destructor
EmuWindow_GLFW::~EmuWindow_GLFW() {
    glfwTerminate();
}

/// Swap buffers to display the next frame
void EmuWindow_GLFW::SwapBuffers() {
    glfwSwapBuffers(m_render_window);
}

/// Polls window events
void EmuWindow_GLFW::PollEvents() {
    glfwPollEvents();
}

/// Makes the GLFW OpenGL context current for the caller thread
void EmuWindow_GLFW::MakeCurrent() {
    glfwMakeContextCurrent(m_render_window);
}

/// Releases (dunno if this is the "right" word) the GLFW context from the caller thread
void EmuWindow_GLFW::DoneCurrent() {
    glfwMakeContextCurrent(nullptr);
}

void EmuWindow_GLFW::ReloadSetKeymaps() {
    for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
        KeyMap::SetKeyMapping({Settings::values.input_mappings[Settings::NativeInput::All[i]], keyboard_id}, Service::HID::pad_mapping[i]);
    }
}

void EmuWindow_GLFW::OnMinimalClientAreaChangeRequest(const std::pair<unsigned,unsigned>& minimal_size) {
    std::pair<int,int> current_size;
    glfwGetWindowSize(m_render_window, &current_size.first, &current_size.second);

    DEBUG_ASSERT((int)minimal_size.first > 0 && (int)minimal_size.second > 0);
    int new_width  = std::max(current_size.first,  (int)minimal_size.first);
    int new_height = std::max(current_size.second, (int)minimal_size.second);

    if (current_size != std::make_pair(new_width, new_height))
        glfwSetWindowSize(m_render_window, new_width, new_height);
}
