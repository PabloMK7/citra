/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    system.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2013-09-26
 * @brief   Emulation of main system
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

#ifndef CORE_SYSTEM_H_
#define CORE_SYSTEM_H_

#include "file_sys/meta_file_system.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace System {

// State of the full emulator
typedef enum {
	STATE_NULL = 0,	///< System is in null state, nothing initialized
	STATE_IDLE,		///< System is in an initialized state, but not running
	STATE_RUNNING,	///< System is running
	STATE_LOADING,	///< System is loading a ROM
	STATE_HALTED,	///< System is halted (error)
	STATE_STALLED,	///< System is stalled (unused)
	STATE_DEBUG,	///< System is in a special debug mode (unused)
	STATE_DIE		///< System is shutting down
} State;

extern volatile State g_state;
extern MetaFileSystem g_ctr_file_system;

void UpdateState(State state);
void Init();
void RunLoopFor(int cycles);
void RunLoopUntil(u64 global_cycles);
void Shutdown();

};

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // CORE_SYSTEM_H_
 