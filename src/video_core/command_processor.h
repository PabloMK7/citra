// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/bit_field.h"
#include "common/common_types.h"

#include "pica.h"

namespace Pica {

namespace CommandProcessor {

union CommandHeader {
    u32 hex;

    BitField< 0, 16, u32> cmd_id;
    BitField<16,  4, u32> parameter_mask;
    BitField<20, 11, u32> extra_data_length;
    BitField<31,  1, u32> group_commands;
};
static_assert(std::is_standard_layout<CommandHeader>::value == true, "CommandHeader does not use standard layout");
static_assert(sizeof(CommandHeader) == sizeof(u32), "CommandHeader has incorrect size!");

void ProcessCommandList(const u32* list, u32 size);

} // namespace

} // namespace
