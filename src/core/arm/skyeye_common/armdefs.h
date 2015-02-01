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

#include <cerrno>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fcntl.h>
#include <sys/stat.h>
#include <sys/types.h>

#include "arm_regformat.h"
#include "common/common_types.h"
#include "common/platform.h"
#include "core/arm/skyeye_common/armmmu.h"
#include "core/arm/skyeye_common/skyeye_defs.h"

#define BITS(s, a, b) ((s << ((sizeof(s) * 8 - 1) - b)) >> (sizeof(s) * 8 - b + a - 1))
#define BIT(s, n) ((s >> (n)) & 1)

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#define LOW 0
#define HIGH 1
#define LOWHIGH 1
#define HIGHLOW 2

//the define of cachetype
#define NONCACHE  0
#define DATACACHE  1
#define INSTCACHE  2

#define POS(i) ( (~(i)) >> 31 )
#define NEG(i) ( (i) >> 31 )

typedef u64 ARMdword;  // must be 64 bits wide
typedef u32 ARMword;   // must be 32 bits wide
typedef u16 ARMhword;  // must be 16 bits wide
typedef u8 ARMbyte;    // must be 8 bits wide
typedef struct ARMul_State ARMul_State;

typedef unsigned ARMul_CPInits(ARMul_State* state);
typedef unsigned ARMul_CPExits(ARMul_State* state);
typedef unsigned ARMul_LDCs(ARMul_State* state, unsigned type, ARMword instr, ARMword value);
typedef unsigned ARMul_STCs(ARMul_State* state, unsigned type, ARMword instr, ARMword* value);
typedef unsigned ARMul_MRCs(ARMul_State* state, unsigned type, ARMword instr, ARMword* value);
typedef unsigned ARMul_MCRs(ARMul_State* state, unsigned type, ARMword instr, ARMword value);
typedef unsigned ARMul_MRRCs(ARMul_State* state, unsigned type, ARMword instr, ARMword* value1, ARMword* value2);
typedef unsigned ARMul_MCRRs(ARMul_State* state, unsigned type, ARMword instr, ARMword value1, ARMword value2);
typedef unsigned ARMul_CDPs(ARMul_State* state, unsigned type, ARMword instr);
typedef unsigned ARMul_CPReads(ARMul_State* state, unsigned reg, ARMword* value);
typedef unsigned ARMul_CPWrites(ARMul_State* state, unsigned reg, ARMword value);

#define VFP_REG_NUM 64
struct ARMul_State
{
    ARMword Emulate;       /* to start and stop emulation */
    unsigned EndCondition; /* reason for stopping */
    unsigned ErrorCode;    /* type of illegal instruction */

    /* Order of the following register should not be modified */
    ARMword Reg[16];            /* the current register file */
    ARMword Cpsr;               /* the current psr */
    ARMword Spsr_copy;
    ARMword phys_pc;
    ARMword Reg_usr[2];
    ARMword Reg_svc[2];         /* R13_SVC R14_SVC */
    ARMword Reg_abort[2];       /* R13_ABORT R14_ABORT */
    ARMword Reg_undef[2];       /* R13 UNDEF R14 UNDEF */
    ARMword Reg_irq[2];         /* R13_IRQ R14_IRQ */
    ARMword Reg_firq[7];        /* R8---R14 FIRQ */
    ARMword Spsr[7];            /* the exception psr's */
    ARMword Mode;               /* the current mode */
    ARMword Bank;               /* the current register bank */
    ARMword exclusive_tag;      /* the address for which the local monitor is in exclusive access mode */
    ARMword exclusive_state;
    ARMword exclusive_result;
    ARMword CP15[VFP_BASE - CP15_BASE];
    ARMword VFP[3]; /* FPSID, FPSCR, and FPEXC */
    /* VFPv2 and VFPv3-D16 has 16 doubleword registers (D0-D16 or S0-S31).
       VFPv3-D32/ASIMD may have up to 32 doubleword registers (D0-D31),
        and only 32 singleword registers are accessible (S0-S31). */
    ARMword ExtReg[VFP_REG_NUM];
    /* ---- End of the ordered registers ---- */
    
    ARMword RegBank[7][16];    /* all the registers */
    //chy:2003-08-19, used in arm xscale
    /* 40 bit accumulator.  We always keep this 64 bits wide,
       and move only 40 bits out of it in an MRA insn.  */
    ARMdword Accumulator;

    ARMword NFlag, ZFlag, CFlag, VFlag, IFFlags;    /* dummy flags for speed */
    unsigned long long int icounter, debug_icounter, kernel_icounter;
    unsigned int shifter_carry_out;

    /* add armv6 flags dyf:2010-08-09 */
    ARMword GEFlag, EFlag, AFlag, QFlag;
    //chy:2003-08-19, used in arm v5e|xscale
    ARMword SFlag;
#ifdef MODET
    ARMword TFlag;        /* Thumb state */
#endif
    ARMword instr, pc, temp;    /* saved register state */
    ARMword loaded, decoded;    /* saved pipeline state */
    //chy 2006-04-12 for ICE breakpoint
    ARMword loaded_addr, decoded_addr;    /* saved pipeline state addr*/
    unsigned int NumScycles, NumNcycles, NumIcycles, NumCcycles, NumFcycles;    /* emulated cycles used */
    unsigned long long NumInstrs;    /* the number of instructions executed */
    unsigned NumInstrsToExecute;

    ARMword currentexaddr;
    ARMword currentexval;
    ARMword currentexvald;
    ARMword servaddr;

    unsigned NextInstr;
    unsigned VectorCatch;                   /* caught exception mask */
    unsigned CallDebug;                     /* set to call the debugger */
    unsigned CanWatch;                      /* set by memory interface if its willing to suffer the
                                               overhead of checking for watchpoints on each memory
                                               access */

    ARMul_CPInits *CPInit[16];              /* coprocessor initialisers */
    ARMul_CPExits *CPExit[16];              /* coprocessor finalisers */
    ARMul_LDCs *LDC[16];                    /* LDC instruction */
    ARMul_STCs *STC[16];                    /* STC instruction */
    ARMul_MRCs *MRC[16];                    /* MRC instruction */
    ARMul_MCRs *MCR[16];                    /* MCR instruction */
    ARMul_MRRCs *MRRC[16];                  /* MRRC instruction */
    ARMul_MCRRs *MCRR[16];                  /* MCRR instruction */
    ARMul_CDPs *CDP[16];                    /* CDP instruction */
    ARMul_CPReads *CPRead[16];              /* Read CP register */
    ARMul_CPWrites *CPWrite[16];            /* Write CP register */
    unsigned char *CPData[16];              /* Coprocessor data */
    unsigned char const *CPRegWords[16];    /* map of coprocessor register sizes */

    unsigned Debug;                         /* show instructions as they are executed */
    unsigned NresetSig;                     /* reset the processor */
    unsigned NfiqSig;
    unsigned NirqSig;

    unsigned abortSig;
    unsigned NtransSig;
    unsigned bigendSig;
    unsigned prog32Sig;
    unsigned data32Sig;
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

    ARMword Vector;              /* synthesize aborts in cycle modes */
    ARMword Aborted;             /* sticky flag for aborts */
    ARMword Reseted;             /* sticky flag for Reset */
    ARMword Inted, LastInted;    /* sticky flags for interrupts */
    ARMword Base;                /* extra hand for base writeback */
    ARMword AbortAddr;           /* to keep track of Prefetch aborts */

    int verbose;        /* non-zero means print various messages like the banner */

    int mmu_inited;

    //chy: 2003-08-11, for different arm core type
    unsigned is_v4;        /* Are we emulating a v4 architecture (or higher) ?  */
    unsigned is_v5;        /* Are we emulating a v5 architecture ?  */
    unsigned is_v5e;       /* Are we emulating a v5e architecture ?  */
    unsigned is_v6;        /* Are we emulating a v6 architecture ?  */
    unsigned is_v7;        /* Are we emulating a v7 architecture ?  */
    unsigned is_XScale;    /* Are we emulating an XScale architecture ?  */
    unsigned is_iWMMXt;    /* Are we emulating an iWMMXt co-processor ?  */
    unsigned is_ep9312;    /* Are we emulating a Cirrus Maverick co-processor ?  */
    unsigned is_pxa27x;    /* Are we emulating a Intel PXA27x co-processor ?  */

    //chy: seems only used in xscale's CP14
    ARMword CP14R0_CCD;    /* used to count 64 clock cycles with CP14 R0 bit 3 set */

    //teawater add for arm2x86 2005.07.05-------------------------------------------
    //arm_arm A2-18
    int abort_model;    //0 Base Restored Abort Model, 1 the Early Abort Model, 2 Base Updated Abort Model

    /*added by ksh in 2005-10-1*/
    cpu_config_t *cpu;

    /* added LPC remap function */
    int vector_remap_flag;
    u32 vector_remap_addr;
    u32 vector_remap_size;

    u32 step;
    u32 cycle;

    /* monitored memory for exclusice access */
    ARMword exclusive_tag_array[128];
    /* 1 means exclusive access and 0 means open access */
    ARMword exclusive_access_state;

    u32 CurrInstr;
    u32 last_pc; /* the last pc executed */
    u32 last_instr; /* the last inst executed */
    u32 WriteAddr[17];
    u32 WriteData[17];
    u32 WritePc[17];
    u32 CurrWrite;
};

typedef ARMul_State arm_core_t;

/***************************************************************************\
*                        Types of ARM we know about                         *
\***************************************************************************/

enum {
    ARM_Fix26_Prop  = 0x01,
    ARM_Nexec_Prop  = 0x02,
    ARM_Debug_Prop  = 0x10,
    ARM_Isync_Prop  = ARM_Debug_Prop,
    ARM_Lock_Prop   = 0x20,
    ARM_v4_Prop     = 0x40,
    ARM_v5_Prop     = 0x80,
    ARM_v6_Prop     = 0xc0,

    ARM_v5e_Prop    = 0x100,
    ARM_XScale_Prop = 0x200,
    ARM_ep9312_Prop = 0x400,
    ARM_iWMMXt_Prop = 0x800,
    ARM_PXA27X_Prop = 0x1000,
    ARM_v7_Prop     = 0x2000,

    // ARM2 family
    ARM2    = ARM_Fix26_Prop,
    ARM2as  = ARM2,
    ARM61   = ARM2,
    ARM3    = ARM2,

    // ARM6 family
    ARM6    = ARM_Lock_Prop,
    ARM60   = ARM6,
    ARM600  = ARM6,
    ARM610  = ARM6,
    ARM620  = ARM6
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

enum {
    USER26MODE   = 0,
    FIQ26MODE    = 1,
    IRQ26MODE    = 2,
    SVC26MODE    = 3,
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
    SYSTEMBANK = USERBANK
};

/***************************************************************************\
*                  Definitons of things in the emulator                     *
\***************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif
extern void ARMul_EmulateInit();
extern void ARMul_Reset(ARMul_State* state);
#ifdef __cplusplus
    }
#endif
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

enum {
    ARMul_CP13_R0_FIQ       = 0x1,
    ARMul_CP13_R0_IRQ       = 0x2,
    ARMul_CP13_R8_PMUS      = 0x1,

    ARMul_CP14_R0_ENABLE    = 0x0001,
    ARMul_CP14_R0_CLKRST    = 0x0004,
    ARMul_CP14_R0_CCD       = 0x0008,
    ARMul_CP14_R0_INTEN0    = 0x0010,
    ARMul_CP14_R0_INTEN1    = 0x0020,
    ARMul_CP14_R0_INTEN2    = 0x0040,
    ARMul_CP14_R0_FLAG0     = 0x0100,
    ARMul_CP14_R0_FLAG1     = 0x0200,
    ARMul_CP14_R0_FLAG2     = 0x0400,
    ARMul_CP14_R10_MOE_IB   = 0x0004,
    ARMul_CP14_R10_MOE_DB   = 0x0008,
    ARMul_CP14_R10_MOE_BT   = 0x000c,
    ARMul_CP15_R1_ENDIAN    = 0x0080,
    ARMul_CP15_R1_ALIGN     = 0x0002,
    ARMul_CP15_R5_X         = 0x0400,
    ARMul_CP15_R5_ST_ALIGN  = 0x0001,
    ARMul_CP15_R5_IMPRE     = 0x0406,
    ARMul_CP15_R5_MMU_EXCPT = 0x0400,
    ARMul_CP15_DBCON_M      = 0x0100,
    ARMul_CP15_DBCON_E1     = 0x000c,
    ARMul_CP15_DBCON_E0     = 0x0003
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
