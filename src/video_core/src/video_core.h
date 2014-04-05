/*!
 * Copyright (C) 2014 Citra Emulator
 *
 * @file    video_core.h
 * @author  bunnei
 * @date    2014-04-05
 * @brief   Main module for new video core
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

#pragma once

#include "common.h"
#include "emu_window.h"
#include "renderer_base.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Video Core namespace

namespace VideoCore {

extern RendererBase*   g_renderer;          ///< Renderer plugin
extern int             g_current_frame;     ///< Current frame

/// Start the video core
void Start();

/// Initialize the video core
void Init(EmuWindow* emu_window);

/// Shutdown the video core
void ShutDown();

} // namespace
