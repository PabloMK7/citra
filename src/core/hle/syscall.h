// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// SVC types

struct MemoryInfo {
    u32 base_address;
    u32 size;
    u32 permission;
    u32 state;
};

struct PageInfo {
    u32 flags;
};

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

enum ResetType {
    RESETTYPE_ONESHOT,
    RESETTYPE_STICKY,
    RESETTYPE_PULSE,
    RESETTYPE_MAX_BIT = (1u << 31),
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Syscall

namespace Syscall {

void Register();

} // namespace
