// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"

namespace HLE {

/// MRC operations (ARM register from coprocessor), decoded as instr[20:27]
enum ARM11_MRC_OPERATION {
    DATA_SYNCHRONIZATION_BARRIER    = 0xE0,
    CALL_GET_THREAD_COMMAND_BUFFER  = 0xE1,
};

/// Call an MRC operation in HLE
u32 CallMRC(ARM11_MRC_OPERATION operation);

} // namespace
