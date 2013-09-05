/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    emuwindow_glfw.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-04-20
 * @brief   Implementation implementation of EmuWindow class for GLFW
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include "common.h"
#include "video_core.h"
#include "emuwindow_glfw.h"
#include "gc_controller.h"
#include "keyboard_input/keyboard_input.h"

static void OnKeyEvent(GLFWwindow win, int key, int action) {
    EmuWindow_GLFW* emuwin = (EmuWindow_GLFW*)glfwGetWindowUserPointer(win);
	input_common::GCController::GCButtonState state;

	if (action == GLFW_PRESS) {
		state = input_common::GCController::PRESSED;
	} else {
		state = input_common::GCController::RELEASED;
	}
    for (int channel = 0; channel < 4 && emuwin->controller_interface(); ++channel) {
		emuwin->controller_interface()->SetControllerStatus(channel, key, state);
    }
}

static void OnWindowSizeEvent(GLFWwindow win, int width, int height) {
    EmuWindow_GLFW* emuwin = (EmuWindow_GLFW*)glfwGetWindowUserPointer(win);
    emuwin->set_client_area_width(width);
    emuwin->set_client_area_height(height);
}

/// EmuWindow_GLFW constructor
EmuWindow_GLFW::EmuWindow_GLFW() {
    // Initialize the window
    if(glfwInit() != GL_TRUE) {
        LOG_ERROR(TVIDEO, "Failed to initialize GLFW! Exiting...");
        exit(E_ERR);
    }
    glfwWindowHint(GLFW_OPENGL_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_OPENGL_VERSION_MINOR, 1);
    render_window_ = glfwCreateWindow(640, 480, GLFW_WINDOWED, "gekko", 0);

    // Setup callbacks
    glfwSetWindowUserPointer(render_window_, this);
    glfwSetKeyCallback(render_window_, OnKeyEvent);
    glfwSetWindowSizeCallback(render_window_, OnWindowSizeEvent);

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
    sprintf(title, "%s (FPS: %02.02f)", window_title_.c_str(), 
        video_core::g_renderer->current_fps());
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
