// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/emu_window.h"

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

void UpdateState(State state);
void Init(EmuWindow* emu_window);
void RunLoopFor(int cycles);
void RunLoopUntil(u64 global_cycles);
void Shutdown();

};
