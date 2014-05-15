// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// SVC structures

struct ThreadContext {
    u32 cpu_registers[13];
    u32 sp;
    u32 lr;
    u32 pc;
    u32 cpsr;
    u32 fpu_registers[32];
    u32 fpscr;
    u32 fpexc;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Syscall

namespace Syscall {

typedef u32 Handle;
typedef s32 Result;

void Register();

} // namespace
