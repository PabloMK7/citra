// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_types.h"

namespace Core {

struct CSTHeader;

struct SaveStateInfo {
    u32 slot;
    u64 time;
    enum class ValidationStatus {
        OK,
        RevisionDismatch,
    } status;
};

constexpr u32 SaveStateSlotCount = 10; // Maximum count of savestate slots

std::vector<SaveStateInfo> ListSaveStates(u64 program_id);

} // namespace Core
