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

#define VFP_REG_NUM 64
struct ARMul_State
{
    u32 Emulate;       // To start and stop emulation

    // Order of the following register should not be modified
    u32 Reg[16];            // The current register file
    u32 Cpsr;               // The current PSR
    u32 Spsr_copy;
    u32 phys_pc;
    u32 Reg_usr[2];
    u32 Reg_svc[2];         // R13_SVC R14_SVC
    u32 Reg_abort[2];       // R13_ABORT R14_ABORT
    u32 Reg_undef[2];       // R13 UNDEF R14 UNDEF
    u32 Reg_irq[2];         // R13_IRQ R14_IRQ
    u32 Reg_firq[7];        // R8---R14 FIRQ
    u32 Spsr[7];            // The exception psr's
    u32 Mode;               // The current mode
    u32 Bank;               // The current register bank
    u32 exclusive_tag;      // The address for which the local monitor is in exclusive access mode
    u32 exclusive_state;
    u32 exclusive_result;
    u32 CP15[CP15_REGISTER_COUNT];

    // FPSID, FPSCR, and FPEXC
    u32 VFP[VFP_SYSTEM_REGISTER_COUNT];
    // VFPv2 and VFPv3-D16 has 16 doubleword registers (D0-D16 or S0-S31).
    // VFPv3-D32/ASIMD may have up to 32 doubleword registers (D0-D31),
    // and only 32 singleword registers are accessible (S0-S31).
    u32 ExtReg[VFP_REG_NUM];
    /* ---- End of the ordered registers ---- */

    u32 NFlag, ZFlag, CFlag, VFlag, IFFlags; // Dummy flags for speed
    unsigned int shifter_carry_out;

    // Add armv6 flags dyf:2010-08-09
    u32 GEFlag, EFlag, AFlag, QFlag;

    u32 TFlag; // Thumb state

    unsigned long long NumInstrs; // The number of instructions executed
    unsigned NumInstrsToExecute;

    unsigned NresetSig; // Reset the processor
    unsigned NfiqSig;
    unsigned NirqSig;

    unsigned abortSig;
    unsigned NtransSig;
    unsigned bigendSig;
    unsigned syscallSig;

    // For differentiating ARM core emulation.
    bool is_v4;     // Are we emulating a v4 architecture (or higher)?
    bool is_v5;     // Are we emulating a v5 architecture?
    bool is_v5e;    // Are we emulating a v5e architecture?
    bool is_v6;     // Are we emulating a v6 architecture?
    bool is_v7;     // Are we emulating a v7 architecture?

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
*                  Definitions of things in the emulator                     *
\***************************************************************************/
void ARMul_Reset(ARMul_State* state);
ARMul_State* ARMul_NewState(ARMul_State* state);

/***************************************************************************\
*            Definitions of things in the co-processor interface             *
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
*               Definitions of things in the host environment                *
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

// Flags for use with the APSR.
enum : u32 {
    NBIT = (1U << 31U),
    ZBIT = (1 << 30),
    CBIT = (1 << 29),
    VBIT = (1 << 28),
    QBIT = (1 << 27),
    JBIT = (1 << 24),
    EBIT = (1 << 9),
    ABIT = (1 << 8),
    IBIT = (1 << 7),
    FBIT = (1 << 6),
    TBIT = (1 << 5),

    // Masks for groups of bits in the APSR.
    MODEBITS = 0x1F,
    INTBITS = 0x1C0,
};

// Values for Emulate.
enum {
    STOP       = 0, // Stop
    CHANGEMODE = 1, // Change mode
    ONCE       = 2, // Execute just one iteration
    RUN        = 3  // Continuous execution
};
