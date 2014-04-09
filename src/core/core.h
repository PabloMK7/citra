// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "core/arm/arm_interface.h"
#include "core/arm/interpreter/armdefs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Core {

extern ARM_Interface*   g_app_core;     ///< ARM11 application core
extern ARM_Interface*   g_sys_core;     ///< ARM11 system (OS) core

////////////////////////////////////////////////////////////////////////////////////////////////////

/// Start the core
void Start();

/// Run the core CPU loop
void RunLoop();

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
