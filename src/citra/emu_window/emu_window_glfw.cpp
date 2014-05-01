// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"

#include "video_core/video_core.h"

#include "citra/citra.h"
#include "citra/emu_window/emu_window_glfw.h"

static void OnKeyEvent(GLFWwindow* win, int key, int action) {
    // TODO(bunnei): ImplementMe
}

static void OnWindowSizeEvent(GLFWwindow* win, int width, int height) {
    EmuWindow_GLFW* emu_window = (EmuWindow_GLFW*)glfwGetWindowUserPointer(win);
    emu_window->SetClientAreaWidth(width);
    emu_window->SetClientAreaHeight(height);
}

/// EmuWindow_GLFW constructor
EmuWindow_GLFW::EmuWindow_GLFW() {
    // Initialize the window
    if(glfwInit() != GL_TRUE) {
        printf("Failed to initialize GLFW! Exiting...");
        exit(1);
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 2);
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    m_render_window = glfwCreateWindow(VideoCore::kScreenTopWidth, 
        (VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight), 
        m_window_title.c_str(), NULL, NULL);

    if (m_render_window == NULL) {
        printf("Failed to create GLFW window! Exiting...");
        exit(1);
    }
    
    // Setup callbacks
    glfwSetWindowUserPointer(m_render_window, this);
    //glfwSetKeyCallback(m_render_window, OnKeyEvent);
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
