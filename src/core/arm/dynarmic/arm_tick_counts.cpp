// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "core/arm/dynarmic/arm_tick_counts.h"

namespace Core {

u64 TicksForInstruction(bool is_thumb, u32 instruction) {
    (void)is_thumb;
    (void)instruction;
    return 1;
}

} // namespace Core
