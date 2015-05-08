/*  armdefs.h -- ARMulator common definitions:  ARM6 Instruction Emulator.
    Copyright (C) 1994 Advanced RISC Machines Ltd.
 
    This program is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.
 
    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.
 
    You should have received a copy of the GNU General Public License
    along with this program; if not, write to the Free Software
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA. */

#pragma once

#include <unordered_map>

#include "common/common_types.h"
#include "core/arm/skyeye_common/arm_regformat.h"

#define BITS(s, a, b) ((s << ((sizeof(s) * 8 - 1) - b)) >> (sizeof(s) * 8 - b + a - 1))
#define BIT(s, n) ((s >> (n)) & 1)

// Signal levels
enum {
    LOW     = 0,
    HIGH    = 1,
    LOWHIGH = 1,
    HIGHLOW = 2
};

// Cache types
enum {
    NONCACHE  = 0,
    DATACACHE = 1,
    INSTCACHE = 2,
};

// Abort models
enum {
    ABORT_BASE_RESTORED = 0,
    ABORT_EARLY         = 1,
    ABORT_BASE_UPDATED  = 2
};

#define POS(i) ( (~(i)) >> 31 )
#define NEG(i) ( (i) >> 31 )

typedef u64 ARMdword;  // must be 64 bits wide
typedef u32 ARMword;   // must be 32 bits wide
typedef u16 ARMhword;  // must be 16 bits wide
typedef u8 ARMbyte;    // must be 8 bits wide

#define VFP_REG_NUM 64
struct ARMul_State
{
    ARMword Emulate;       // To start and stop emulation

    // Order of the following register should not be modified
    ARMword Reg[16];            // The current register file
    ARMword Cpsr;               // The current PSR
    ARMword Spsr_copy;
    ARMword phys_pc;
    ARMword Reg_usr[2];
    ARMword Reg_svc[2];         // R13_SVC R14_SVC
    ARMword Reg_abort[2];       // R13_ABORT R14_ABORT
    ARMword Reg_undef[2];       // R13 UNDEF R14 UNDEF
    ARMword Reg_irq[2];         // R13_IRQ R14_IRQ
    ARMword Reg_firq[7];        // R8---R14 FIRQ
    ARMword Spsr[7];            // The exception psr's
    ARMword Mode;               // The current mode
    ARMword Bank;               // The current register bank
    ARMword exclusive_tag;      // The address for which the local monitor is in exclusive access mode
    ARMword exclusive_state;
    ARMword exclusive_result;
    ARMword CP15[CP15_REGISTER_COUNT];

    // FPSID, FPSCR, and FPEXC
    ARMword VFP[VFP_SYSTEM_REGISTER_COUNT];
    // VFPv2 and VFPv3-D16 has 16 doubleword registers (D0-D16 or S0-S31).
    // VFPv3-D32/ASIMD may have up to 32 doubleword registers (D0-D31),
    // and only 32 singleword registers are accessible (S0-S31).
    ARMword ExtReg[VFP_REG_NUM];
    /* ---- End of the ordered registers ---- */

    ARMword NFlag, ZFlag, CFlag, VFlag, IFFlags; // Dummy flags for speed
    unsigned int shifter_carry_out;

    // Add armv6 flags dyf:2010-08-09
    ARMword GEFlag, EFlag, AFlag, QFlag;

    ARMword TFlag; // Thumb state

    unsigned long long NumInstrs; // The number of instructions executed
    unsigned NumInstrsToExecute;

    unsigned NresetSig; // Reset the processor
    unsigned NfiqSig;
    unsigned NirqSig;

    unsigned abortSig;
    unsigned NtransSig;
    unsigned bigendSig;
    unsigned syscallSig;

/* 2004-05-09 chy
----------------------------------------------------------
read ARM Architecture Reference Manual
2.6.5 Data Abort
There are three Abort Model in ARM arch.

Early Abort Model: used in some ARMv3 and earlier implementations. In this
model, base register wirteback occurred for LDC,LDM,STC,STM instructions, and
the base register was unchanged for all other instructions. (oldest)

Base Restored Abort Model: If a Data Abort occurs in an instruction which
specifies base register writeback, the value in the base register is
unchanged. (strongarm, xscale)

Base Updated Abort Model: If a Data Abort occurs in an instruction which
specifies base register writeback, the base register writeback still occurs.
(arm720T)

read PART B
chap2 The System Control Coprocessor  CP15
2.4 Register1:control register
L(bit 6): in some ARMv3 and earlier implementations, the abort model of the
processor could be configured:
0=early Abort Model Selected(now obsolete)
1=Late Abort Model selceted(same as Base Updated Abort Model)

on later processors, this bit reads as 1 and ignores writes.
-------------------------------------------------------------
So, if lateabtSig=1, then it means Late Abort Model(Base Updated Abort Model)
    if lateabtSig=0, then it means Base Restored Abort Model
*/
    unsigned lateabtSig;

    // For differentiating ARM core emulaiton.
    bool is_v4;     // Are we emulating a v4 architecture (or higher)?
    bool is_v5;     // Are we emulating a v5 architecture?
    bool is_v5e;    // Are we emulating a v5e architecture?
    bool is_v6;     // Are we emulating a v6 architecture?
    bool is_v7;     // Are we emulating a v7 architecture?

    // ARM_ARM A2-18
    // 0 Base Restored Abort Model, 1 the Early Abort Model, 2 Base Updated Abort Model
    int abort_model;

    // TODO(bunnei): Move this cache to a better place - it should be per codeset (likely per
    // process for our purposes), not per ARMul_State (which tracks CPU core state).
    std::unordered_map<u32, int> instruction_cache;
};

/***************************************************************************\
*                        Types of ARM we know about                         *
\***************************************************************************/

enum {
    ARM_v4_Prop  = 0x01,
    ARM_v5_Prop  = 0x02,
    ARM_v5e_Prop = 0x04,
    ARM_v6_Prop  = 0x08,
    ARM_v7_Prop  = 0x10,
};

/***************************************************************************\
*                      The hardware vector addresses                        *
\***************************************************************************/

enum {
    ARMResetV          = 0,
    ARMUndefinedInstrV = 4,
    ARMSWIV            = 8,
    ARMPrefetchAbortV  = 12,
    ARMDataAbortV      = 16,
    ARMAddrExceptnV    = 20,
    ARMIRQV            = 24,
    ARMFIQV            = 28,
    ARMErrorV          = 32, // This is an offset, not an address!

    ARMul_ResetV          = ARMResetV,
    ARMul_UndefinedInstrV = ARMUndefinedInstrV,
    ARMul_SWIV            = ARMSWIV,
    ARMul_PrefetchAbortV  = ARMPrefetchAbortV,
    ARMul_DataAbortV      = ARMDataAbortV,
    ARMul_AddrExceptnV    = ARMAddrExceptnV,
    ARMul_IRQV            = ARMIRQV,
    ARMul_FIQV            = ARMFIQV
};

/***************************************************************************\
*                          Mode and Bank Constants                          *
\***************************************************************************/

enum PrivilegeMode {
    USER32MODE   = 16,
    FIQ32MODE    = 17,
    IRQ32MODE    = 18,
    SVC32MODE    = 19,
    ABORT32MODE  = 23,
    UNDEF32MODE  = 27,
    SYSTEM32MODE = 31
};

enum {
    USERBANK   = 0,
    FIQBANK    = 1,
    IRQBANK    = 2,
    SVCBANK    = 3,
    ABORTBANK  = 4,
    UNDEFBANK  = 5,
    DUMMYBANK  = 6,
    SYSTEMBANK = 7
};

/***************************************************************************\
*                  Definitons of things in the emulator                     *
\***************************************************************************/
extern void ARMul_Reset(ARMul_State* state);
extern ARMul_State* ARMul_NewState(ARMul_State* state);

/***************************************************************************\
*            Definitons of things in the co-processor interface             *
\***************************************************************************/

enum {
    ARMul_FIRST     = 0,
    ARMul_TRANSFER  = 1,
    ARMul_BUSY      = 2,
    ARMul_DATA      = 3,
    ARMul_INTERRUPT = 4,
    ARMul_DONE      = 0,
    ARMul_CANT      = 1,
    ARMul_INC       = 3
};

/***************************************************************************\
*               Definitons of things in the host environment                *
\***************************************************************************/

enum ConditionCode {
    EQ = 0,
    NE = 1,
    CS = 2,
    CC = 3,
    MI = 4,
    PL = 5,
    VS = 6,
    VC = 7,
    HI = 8,
    LS = 9,
    GE = 10,
    LT = 11,
    GT = 12,
    LE = 13,
    AL = 14,
    NV = 15,
};

extern bool AddOverflow(ARMword, ARMword, ARMword);
extern bool SubOverflow(ARMword, ARMword, ARMword);

extern void ARMul_SelectProcessor(ARMul_State*, unsigned);

extern u32 AddWithCarry(u32, u32, u32, bool*, bool*);
extern bool ARMul_AddOverflowQ(ARMword, ARMword);

extern u8 ARMul_SignedSaturatedAdd8(u8, u8);
extern u8 ARMul_SignedSaturatedSub8(u8, u8);
extern u16 ARMul_SignedSaturatedAdd16(u16, u16);
extern u16 ARMul_SignedSaturatedSub16(u16, u16);

extern u8 ARMul_UnsignedSaturatedAdd8(u8, u8);
extern u16 ARMul_UnsignedSaturatedAdd16(u16, u16);
extern u8 ARMul_UnsignedSaturatedSub8(u8, u8);
extern u16 ARMul_UnsignedSaturatedSub16(u16, u16);
extern u8 ARMul_UnsignedAbsoluteDifference(u8, u8);
extern u32 ARMul_SignedSatQ(s32, u8, bool*);
extern u32 ARMul_UnsignedSatQ(s32, u8, bool*);

extern bool InBigEndianMode(ARMul_State*);
extern bool InAPrivilegedMode(ARMul_State*);

extern u32 ReadCP15Register(ARMul_State* cpu, u32 crn, u32 opcode_1, u32 crm, u32 opcode_2);
extern void WriteCP15Register(ARMul_State* cpu, u32 value, u32 crn, u32 opcode_1, u32 crm, u32 opcode_2);
