// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"
#include "video_core/video_core.h"
#include "citra/emu_window/emu_window_glfw.h"

static void OnKeyEvent(GLFWwindow* win, int key, int action) {
    // TODO(bunnei): ImplementMe
}

static void OnWindowSizeEvent(GLFWwindow* win, int width, int height) {
    EmuWindow_GLFW* emuwin = (EmuWindow_GLFW*)glfwGetWindowUserPointer(win);
    emuwin->set_client_area_width(width);
    emuwin->set_client_area_height(height);
}

/// EmuWindow_GLFW constructor
EmuWindow_GLFW::EmuWindow_GLFW() {
    // Initialize the window
    if(glfwInit() != GL_TRUE) {
        printf("Failed to initialize GLFW! Exiting...");
        exit(1);
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 1);
    render_window_ = glfwCreateWindow(VideoCore::kScreenTopWidth, 
        (VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight), "citra", NULL, NULL);

    // Setup callbacks
    glfwSetWindowUserPointer(render_window_, this);
    //glfwSetKeyCallback(render_window_, OnKeyEvent);
    //glfwSetWindowSizeCallback(render_window_, OnWindowSizeEvent);

    DoneCurrent();
}

/// EmuWindow_GLFW destructor
EmuWindow_GLFW::~EmuWindow_GLFW() {
    glfwTerminate();
}

/// Swap buffers to display the next frame
void EmuWindow_GLFW::SwapBuffers() {
    glfwSwapBuffers(render_window_);
}

/// Polls window events
void EmuWindow_GLFW::PollEvents() {
    // TODO(ShizZy): Does this belong here? This is a reasonable place to update the window title
    //  from the main thread, but this should probably be in an event handler...
    static char title[128];
    sprintf(title, "%s (FPS: %02.02f)", window_title_.c_str(), 0.0f);
    glfwSetWindowTitle(render_window_, title);

    glfwPollEvents();
}

/// Makes the GLFW OpenGL context current for the caller thread
void EmuWindow_GLFW::MakeCurrent() {
    glfwMakeContextCurrent(render_window_);
}

/// Releases (dunno if this is the "right" word) the GLFW context from the caller thread
void EmuWindow_GLFW::DoneCurrent() {
    glfwMakeContextCurrent(NULL);
}
