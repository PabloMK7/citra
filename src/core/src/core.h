/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    core.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2013-09-04
 * @brief   Core of emulator
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

#ifndef CORE_CORE_H_
#define CORE_CORE_H_

////////////////////////////////////////////////////////////////////////////////////////////////////

#include "common.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

class EmuWindow;

namespace Core {

// State of the full emulator
typedef enum {
	SYS_NULL = 0,   ///< System is in null state, nothing initialized
	SYS_IDLE,       ///< System is in an initialized state, but not running
	SYS_RUNNING,    ///< System is running
	SYS_LOADING,    ///< System is loading a ROM
	SYS_HALTED,     ///< System is halted (error)
	SYS_STALLED,    ///< System is stalled (unused)
	SYS_DEBUG,      ///< System is in a special debug mode (unused)
	SYS_DIE         ///< System is shutting down
} SystemState;


/// Start the core
void Start();

/// Kill the core
void Kill();

/// Stop the core
void Stop();

/// Initialize the core
int Init(EmuWindow* emu_window);

extern SystemState	g_state;	///< State of the emulator
extern bool			g_started;	///< Whether or not the emulator has been started

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // CORE_CORE_H_
