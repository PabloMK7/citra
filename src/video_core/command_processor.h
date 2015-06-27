// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <type_traits>

#include "common/bit_field.h"
#include "common/common_types.h"

namespace Pica {

namespace CommandProcessor {

union CommandHeader {
    u32 hex;

    BitField< 0, 16, u32> cmd_id;

    // parameter_mask:
    // Mask applied to the input value to make it possible to update
    // parts of a register without overwriting its other fields.
    // first bit:  0x000000FF
    // second bit: 0x0000FF00
    // third bit:  0x00FF0000
    // fourth bit: 0xFF000000
    BitField<16,  4, u32> parameter_mask;

    BitField<20, 11, u32> extra_data_length;

    BitField<31,  1, u32> group_commands;
};
static_assert(std::is_standard_layout<CommandHeader>::value == true,
              "CommandHeader does not use standard layout");
static_assert(sizeof(CommandHeader) == sizeof(u32), "CommandHeader has incorrect size!");

void ProcessCommandList(const u32* list, u32 size);

} // namespace

} // namespace
