// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include "common/common_types.h"

class ARM_Interface;

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Core {

struct ThreadContext {
    u32 cpu_registers[13];
    u32 sp;
    u32 lr;
    u32 pc;
    u32 cpsr;
    u32 fpu_registers[64];
    u32 fpscr;
    u32 fpexc;
};

extern std::unique_ptr<ARM_Interface> g_app_core; ///< ARM11 application core
extern std::unique_ptr<ARM_Interface> g_sys_core; ///< ARM11 system (OS) core

////////////////////////////////////////////////////////////////////////////////////////////////////

/// Start the core
void Start();

/**
 * Run the core CPU loop
 * This function runs the core for the specified number of CPU instructions before trying to update
 * hardware. This is much faster than SingleStep (and should be equivalent), as the CPU is not
 * required to do a full dispatch with each instruction. NOTE: the number of instructions requested
 * is not guaranteed to run, as this will be interrupted preemptively if a hardware update is
 * requested (e.g. on a thread switch).
 */
void RunLoop(int tight_loop = 1000);

/// Step the CPU one instruction
void SingleStep();

/// Halt the core
void Halt(const char* msg);

/// Kill the core
void Stop();

/// Initialize the core
void Init();

/// Shutdown the core
void Shutdown();

} // namespace
