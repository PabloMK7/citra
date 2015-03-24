// Copyright 2012 Michael Kang, 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

int decode_arm_instr(uint32_t instr, int32_t *idx);

enum DECODE_STATUS {
    DECODE_SUCCESS,
    DECODE_FAILURE
};

struct instruction_set_encoding_item {
    const char *name;
    int attribute_value;
    int version;
    u32 content[21];
};

typedef struct instruction_set_encoding_item ISEITEM;

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

extern const ISEITEM arm_instruction[];
