// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "core/arm/arm_interface.h"
#include "core/arm/skyeye_common/armdefs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Core {

extern ARM_Interface*   g_app_core;     ///< ARM11 application core
extern ARM_Interface*   g_sys_core;     ///< ARM11 system (OS) core

////////////////////////////////////////////////////////////////////////////////////////////////////

/// Start the core
void Start();

/**
 * Run the core CPU loop
 * This function loops for 100 instructions in the CPU before trying to update hardware. This is a
 * little bit faster than SingleStep, and should be pretty much equivalent. The number of
 * instructions chosen is fairly arbitrary, however a large number will more drastically affect the
 * frequency of GSP interrupts and likely break things. The point of this is to just loop in the CPU
 * for more than 1 instruction to reduce overhead and make it a little bit faster...
 */
void RunLoop(int tight_loop=100);

/// Step the CPU one instruction
void SingleStep();

/// Halt the core
void Halt(const char *msg);

/// Kill the core
void Stop();

/// Initialize the core
int Init();

/// Shutdown the core
void Shutdown();

} // namespace
