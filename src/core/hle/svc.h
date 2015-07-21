// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
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

enum ResetType {
    RESETTYPE_ONESHOT,
    RESETTYPE_STICKY,
    RESETTYPE_PULSE,
    RESETTYPE_MAX_BIT = (1u << 31),
};

enum ArbitrationType {
    ARBITRATIONTYPE_SIGNAL,
    ARBITRATIONTYPE_WAIT_IF_LESS_THAN,
    ARBITRATIONTYPE_DECREMENT_AND_WAIT_IF_LESS_THAN,
    ARBITRATIONTYPE_WAIT_IF_LESS_THAN_WITH_TIMEOUT,
    ARBITRATIONTYPE_DECREMENT_AND_WAIT_IF_LESS_THAN_WITH_TIMEOUT,
    ARBITRATIONTYPE_MAX_BIT = (1u << 31)
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SVC

namespace SVC {

void CallSVC(u32 immediate);

} // namespace
