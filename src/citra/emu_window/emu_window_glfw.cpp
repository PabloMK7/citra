// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"

#include "video_core/video_core.h"

#include "citra/emu_window/emu_window_glfw.h"

static const KeyMap::DefaultKeyMapping default_key_map[] = {
    { KeyMap::CitraKey(GLFW_KEY_A), HID_User::PAD_A },
    { KeyMap::CitraKey(GLFW_KEY_B), HID_User::PAD_B },
    { KeyMap::CitraKey(GLFW_KEY_BACKSLASH), HID_User::PAD_SELECT },
    { KeyMap::CitraKey(GLFW_KEY_ENTER), HID_User::PAD_START },
    { KeyMap::CitraKey(GLFW_KEY_RIGHT), HID_User::PAD_RIGHT },
    { KeyMap::CitraKey(GLFW_KEY_LEFT), HID_User::PAD_LEFT },
    { KeyMap::CitraKey(GLFW_KEY_UP), HID_User::PAD_UP },
    { KeyMap::CitraKey(GLFW_KEY_DOWN), HID_User::PAD_DOWN },
    { KeyMap::CitraKey(GLFW_KEY_R), HID_User::PAD_R },
    { KeyMap::CitraKey(GLFW_KEY_L), HID_User::PAD_L },
    { KeyMap::CitraKey(GLFW_KEY_X), HID_User::PAD_X },
    { KeyMap::CitraKey(GLFW_KEY_Y), HID_User::PAD_Y },
    { KeyMap::CitraKey(GLFW_KEY_H), HID_User::PAD_CIRCLE_RIGHT },
    { KeyMap::CitraKey(GLFW_KEY_F), HID_User::PAD_CIRCLE_LEFT },
    { KeyMap::CitraKey(GLFW_KEY_T), HID_User::PAD_CIRCLE_UP },
    { KeyMap::CitraKey(GLFW_KEY_G), HID_User::PAD_CIRCLE_DOWN },
};


static void OnKeyEvent(GLFWwindow* win, int key, int scancode, int action, int mods) {
    if (action == GLFW_PRESS) {
        EmuWindow::KeyPressed(KeyMap::CitraKey(key));
    }

    if (action == GLFW_RELEASE) {
        EmuWindow::KeyReleased(KeyMap::CitraKey(key));
    }
    HID_User::PADUpdateComplete();
}

static void OnWindowSizeEvent(GLFWwindow* win, int width, int height) {
    EmuWindow_GLFW* emu_window = (EmuWindow_GLFW*)glfwGetWindowUserPointer(win);
    emu_window->SetClientAreaWidth(width);
    emu_window->SetClientAreaHeight(height);
}

/// EmuWindow_GLFW constructor
EmuWindow_GLFW::EmuWindow_GLFW() {

    // Set default key mappings
    for (int i = 0; i < ARRAY_SIZE(default_key_map); i++) {
        KeyMap::SetKeyMapping(default_key_map[i].key, default_key_map[i].state);
    }

    // Initialize the window
    if(glfwInit() != GL_TRUE) {
        printf("Failed to initialize GLFW! Exiting...");
        exit(1);
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
	
#if EMU_PLATFORM == PLATFORM_MACOSX
    // GLFW on OSX requires these window hints to be set to create a 3.2+ GL context.
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
#endif
	
    m_render_window = glfwCreateWindow(VideoCore::kScreenTopWidth, 
        (VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight), 
        m_window_title.c_str(), NULL, NULL);

    if (m_render_window == NULL) {
        printf("Failed to create GLFW window! Exiting...");
        exit(1);
    }
    
    // Setup callbacks
    glfwSetWindowUserPointer(m_render_window, this);
    glfwSetKeyCallback(m_render_window, OnKeyEvent);
    //glfwSetWindowSizeCallback(m_render_window, OnWindowSizeEvent);

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
    glfwMakeContextCurrent(NULL);
}
