// Copyright 2012 Michael Kang, 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#define BITS(a,b)   ((instr >> (a)) & ((1 << (1+(b)-(a)))-1))
#define BIT(n)      ((instr >> (n)) & 1)

// For MUL instructions
#define RDHi        ((instr >> 16) & 0xF)
#define RDLo        ((instr >> 12) & 0xF)
#define MUL_RD      ((instr >> 16) & 0xF)
#define MUL_RN      ((instr >> 12) & 0xF)
#define RS          ((instr >> 8) & 0xF)
#define RD          ((instr >> 12) & 0xF)
#define RN          ((instr >> 16) & 0xF)
#define RM          (instr & 0xF)

// CP15 registers
#define OPCODE_1    BITS(21, 23)
#define CRn         BITS(16, 19)
#define CRm         BITS(0, 3)
#define OPCODE_2    BITS(5, 7)

#define I           BIT(25)
#define S           BIT(20)

#define             SHIFT BITS(5,6)
#define             SHIFT_IMM BITS(7,11)
#define             IMMH BITS(8,11)
#define             IMML BITS(0,3)

#define LSPBIT      BIT(24)
#define LSUBIT      BIT(23)
#define LSBBIT      BIT(22)
#define LSWBIT      BIT(21)
#define LSLBIT      BIT(20)
#define LSSHBITS    BITS(5,6)
#define OFFSET12    BITS(0,11)
#define SBIT        BIT(20)
#define DESTReg     (BITS (12, 15))

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
