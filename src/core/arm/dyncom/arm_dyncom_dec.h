// Copyright 2012 Michael Kang, 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

enum class ARMDecodeStatus {
    SUCCESS,
    FAILURE
};

ARMDecodeStatus DecodeARMInstruction(u32 instr, s32* idx);

struct InstructionSetEncodingItem {
    const char *name;
    int attribute_value;
    int version;
    u32 content[21];
};

// ARM versions
enum {
    INVALID = 0,
    ARMALL,
    ARMV4,
    ARMV4T,
    ARMV5T,
    ARMV5TE,
    ARMV5TEJ,
    ARMV6,
    ARM1176JZF_S,
    ARMVFP2,
    ARMVFP3,
    ARMV6K,
};

extern const InstructionSetEncodingItem arm_instruction[];
