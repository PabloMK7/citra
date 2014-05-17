// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/bit_field.h"
#include "common/common_types.h"

namespace Pica {

enum class CommandId : u32
{
    ViewportSizeX            =  0x41,
    ViewportInvSizeX         =  0x42,
    ViewportSizeY            =  0x43,
    ViewportInvSizeY         =  0x44,
    ViewportCorner           =  0x68,
    DepthBufferFormat        = 0x116,
    ColorBufferFormat        = 0x117,
    DepthBufferAddress       = 0x11C,
    ColorBufferAddress       = 0x11D,
    ColorBufferSize          = 0x11E,
};

union CommandHeader {
    u32 hex;

    BitField< 0, 16, CommandId> cmd_id;
    BitField<16,  4, u32> parameter_mask;
    BitField<20, 11, u32> extra_data_length;
    BitField<31,  1, u32> group_commands;
};

}
