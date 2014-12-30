// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <GLFW/glfw3.h>

#include "common/common.h"

#include "video_core/video_core.h"

#include "core/settings.h"

#include "citra/emu_window/emu_window_glfw.h"

EmuWindow_GLFW* EmuWindow_GLFW::GetEmuWindow(GLFWwindow* win) {
    return static_cast<EmuWindow_GLFW*>(glfwGetWindowUserPointer(win));
}

/// Called by GLFW when a key event occurs
void EmuWindow_GLFW::OnKeyEvent(GLFWwindow* win, int key, int scancode, int action, int mods) {

    int keyboard_id = GetEmuWindow(win)->keyboard_id;

    if (action == GLFW_PRESS) {
        EmuWindow::KeyPressed({key, keyboard_id});
    } else if (action == GLFW_RELEASE) {
        EmuWindow::KeyReleased({key, keyboard_id});
    }

    HID_User::PadUpdateComplete();
}

/// Whether the window is still open, and a close request hasn't yet been sent
const bool EmuWindow_GLFW::IsOpen() {
    return glfwWindowShouldClose(m_render_window) == 0;
}

void EmuWindow_GLFW::OnFramebufferResizeEvent(GLFWwindow* win, int width, int height) {
    _dbg_assert_(Frontend, width > 0);
    _dbg_assert_(Frontend, height > 0);

    GetEmuWindow(win)->NotifyFramebufferSizeChanged(std::pair<unsigned,unsigned>(width, height));
}

void EmuWindow_GLFW::OnClientAreaResizeEvent(GLFWwindow* win, int width, int height) {
    _dbg_assert_(Frontend, width > 0);
    _dbg_assert_(Frontend, height > 0);

    // NOTE: GLFW provides no proper way to set a minimal window size.
    //       Hence, we just ignore the corresponding EmuWindow hint.

    GetEmuWindow(win)->NotifyClientAreaSizeChanged(std::pair<unsigned,unsigned>(width, height));
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
    KeyMap::SetKeyMapping({Settings::values.pad_a_key,      keyboard_id}, HID_User::PAD_A);
    KeyMap::SetKeyMapping({Settings::values.pad_b_key,      keyboard_id}, HID_User::PAD_B);
    KeyMap::SetKeyMapping({Settings::values.pad_select_key, keyboard_id}, HID_User::PAD_SELECT);
    KeyMap::SetKeyMapping({Settings::values.pad_start_key,  keyboard_id}, HID_User::PAD_START);
    KeyMap::SetKeyMapping({Settings::values.pad_dright_key, keyboard_id}, HID_User::PAD_RIGHT);
    KeyMap::SetKeyMapping({Settings::values.pad_dleft_key,  keyboard_id}, HID_User::PAD_LEFT);
    KeyMap::SetKeyMapping({Settings::values.pad_dup_key,    keyboard_id}, HID_User::PAD_UP);
    KeyMap::SetKeyMapping({Settings::values.pad_ddown_key,  keyboard_id}, HID_User::PAD_DOWN);
    KeyMap::SetKeyMapping({Settings::values.pad_r_key,      keyboard_id}, HID_User::PAD_R);
    KeyMap::SetKeyMapping({Settings::values.pad_l_key,      keyboard_id}, HID_User::PAD_L);
    KeyMap::SetKeyMapping({Settings::values.pad_x_key,      keyboard_id}, HID_User::PAD_X);
    KeyMap::SetKeyMapping({Settings::values.pad_y_key,      keyboard_id}, HID_User::PAD_Y);
    KeyMap::SetKeyMapping({Settings::values.pad_sright_key, keyboard_id}, HID_User::PAD_CIRCLE_RIGHT);
    KeyMap::SetKeyMapping({Settings::values.pad_sleft_key,  keyboard_id}, HID_User::PAD_CIRCLE_LEFT);
    KeyMap::SetKeyMapping({Settings::values.pad_sup_key,    keyboard_id}, HID_User::PAD_CIRCLE_UP);
    KeyMap::SetKeyMapping({Settings::values.pad_sdown_key,  keyboard_id}, HID_User::PAD_CIRCLE_DOWN);
}

void EmuWindow_GLFW::OnMinimalClientAreaChangeRequest(const std::pair<unsigned,unsigned>& minimal_size) {
    std::pair<int,int> current_size;
    glfwGetWindowSize(m_render_window, &current_size.first, &current_size.second);

    _dbg_assert_(Frontend, (int)minimal_size.first > 0 && (int)minimal_size.second > 0);
    int new_width  = std::max(current_size.first,  (int)minimal_size.first);
    int new_height = std::max(current_size.second, (int)minimal_size.second);

    if (current_size != std::make_pair(new_width, new_height))
        glfwSetWindowSize(m_render_window, new_width, new_height);
}
