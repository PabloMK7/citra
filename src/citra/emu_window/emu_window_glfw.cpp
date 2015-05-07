// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <GLFW/glfw3.h>

#include "common/logging/log.h"

#include "video_core/video_core.h"

#include "core/settings.h"

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
    KeyMap::SetKeyMapping({Settings::values.pad_a_key,      keyboard_id}, Service::HID::PAD_A);
    KeyMap::SetKeyMapping({Settings::values.pad_b_key,      keyboard_id}, Service::HID::PAD_B);
    KeyMap::SetKeyMapping({Settings::values.pad_select_key, keyboard_id}, Service::HID::PAD_SELECT);
    KeyMap::SetKeyMapping({Settings::values.pad_start_key,  keyboard_id}, Service::HID::PAD_START);
    KeyMap::SetKeyMapping({Settings::values.pad_dright_key, keyboard_id}, Service::HID::PAD_RIGHT);
    KeyMap::SetKeyMapping({Settings::values.pad_dleft_key,  keyboard_id}, Service::HID::PAD_LEFT);
    KeyMap::SetKeyMapping({Settings::values.pad_dup_key,    keyboard_id}, Service::HID::PAD_UP);
    KeyMap::SetKeyMapping({Settings::values.pad_ddown_key,  keyboard_id}, Service::HID::PAD_DOWN);
    KeyMap::SetKeyMapping({Settings::values.pad_r_key,      keyboard_id}, Service::HID::PAD_R);
    KeyMap::SetKeyMapping({Settings::values.pad_l_key,      keyboard_id}, Service::HID::PAD_L);
    KeyMap::SetKeyMapping({Settings::values.pad_x_key,      keyboard_id}, Service::HID::PAD_X);
    KeyMap::SetKeyMapping({Settings::values.pad_y_key,      keyboard_id}, Service::HID::PAD_Y);

    KeyMap::SetKeyMapping({Settings::values.pad_zl_key,     keyboard_id}, Service::HID::PAD_ZL);
    KeyMap::SetKeyMapping({Settings::values.pad_zr_key,     keyboard_id}, Service::HID::PAD_ZR);

    // KeyMap::SetKeyMapping({Settings::values.pad_touch_key,  keyboard_id}, Service::HID::PAD_TOUCH);

    KeyMap::SetKeyMapping({Settings::values.pad_cright_key, keyboard_id}, Service::HID::PAD_C_RIGHT);
    KeyMap::SetKeyMapping({Settings::values.pad_cleft_key,  keyboard_id}, Service::HID::PAD_C_LEFT);
    KeyMap::SetKeyMapping({Settings::values.pad_cup_key,    keyboard_id}, Service::HID::PAD_C_UP);
    KeyMap::SetKeyMapping({Settings::values.pad_cdown_key,  keyboard_id}, Service::HID::PAD_C_DOWN);
    KeyMap::SetKeyMapping({Settings::values.pad_sright_key, keyboard_id}, Service::HID::PAD_CIRCLE_RIGHT);
    KeyMap::SetKeyMapping({Settings::values.pad_sleft_key,  keyboard_id}, Service::HID::PAD_CIRCLE_LEFT);
    KeyMap::SetKeyMapping({Settings::values.pad_sup_key,    keyboard_id}, Service::HID::PAD_CIRCLE_UP);
    KeyMap::SetKeyMapping({Settings::values.pad_sdown_key,  keyboard_id}, Service::HID::PAD_CIRCLE_DOWN);
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
