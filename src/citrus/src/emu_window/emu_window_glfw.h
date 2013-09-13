/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    emu_window_glfw.h
 * @author  ShizZy <shizzy@6bit.net>
 * @date    2013-09-04
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

#ifndef CITRUS_EMUWINDOW_GLFW_
#define CITRUS_EMUWINDOW_GLFW_

#include <GL/glew.h>
#include <GLFW/glfw3.h>

#include "emu_window.h"

class EmuWindow_GLFW : public EmuWindow {
public:
    EmuWindow_GLFW();
    ~EmuWindow_GLFW();

    /// Swap buffers to display the next frame
    void SwapBuffers();

	/// Polls window events
	void PollEvents();

    /// Makes the graphics context current for the caller thread
    void MakeCurrent();
    
    /// Releases (dunno if this is the "right" word) the GLFW context from the caller thread
    void DoneCurrent();

	GLFWwindow* render_window_;      ///< Internal GLFW render window

private:

};

#endif // CITRUS_EMUWINDOW_GLFW_
