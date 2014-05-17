// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"

namespace HLE {

/// Coprocessor operations
enum CoprocessorOperation {
    DATA_SYNCHRONIZATION_BARRIER    = 0xE0,
    CALL_GET_THREAD_COMMAND_BUFFER  = 0xE1,
};

/// Call an MRC (move to ARM register from coprocessor) instruction in HLE
s32 CallMRC(u32 instruction);

} // namespace
