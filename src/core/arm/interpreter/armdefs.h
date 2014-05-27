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

#ifndef _ARMDEFS_H_
#define _ARMDEFS_H_

#include <stdio.h>
#include <stdlib.h>
#include <errno.h>

#include "common/platform.h"

//teawater add for arm2x86 2005.02.14-------------------------------------------
// koodailar remove it for mingw 2005.12.18----------------
//anthonylee modify it for portable 2007.01.30
//#include "portable/mman.h"

#include "arm_regformat.h"
#include "common/platform.h"
#include "skyeye_defs.h"

//AJ2D--------------------------------------------------------------------------

//teawater add for arm2x86 2005.07.03-------------------------------------------

#include <sys/types.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#if EMU_PLATFORM == PLATFORM_LINUX
#include <unistd.h>
#endif
#include <errno.h>
#include <sys/stat.h>
#include <fcntl.h>

//#include <memory_space.h>
//AJ2D--------------------------------------------------------------------------
#if 0
#if 0
#define DIFF_STATE 1
#define __FOLLOW_MODE__ 0
#else
#define DIFF_STATE 0
#define __FOLLOW_MODE__ 1
#endif
#endif

#ifndef FALSE
#define FALSE 0
#define TRUE 1
#endif

#define LOW 0
#define HIGH 1
#define LOWHIGH 1
#define HIGHLOW 2

#ifndef u8
#define u8      unsigned char
#define u16     unsigned short
#define u32     unsigned int
#define u64     unsigned long long
#endif /*u8 */

//teawater add DBCT_TEST_SPEED 2005.10.04---------------------------------------
#include <signal.h>

#include "common/platform.h"

#if EMU_PLATFORM == PLATFORM_LINUX
#include <sys/time.h>
#endif

//#define DBCT_TEST_SPEED
#define DBCT_TEST_SPEED_SEC    10
//AJ2D--------------------------------------------------------------------------

//teawater add compile switch for DBCT GDB RSP function 2005.10.21--------------
//#define DBCT_GDBRSP
//AJ2D--------------------------------------------------------------------------

//#include <skyeye_defs.h>
//#include <skyeye_types.h>

#define ARM_BYTE_TYPE         0
#define ARM_HALFWORD_TYPE     1
#define ARM_WORD_TYPE         2

//the define of cachetype
#define NONCACHE  0
#define DATACACHE  1
#define INSTCACHE  2

#ifndef __STDC__
typedef char *VoidStar;
#endif

typedef unsigned long long ARMdword;    /* must be 64 bits wide */
typedef unsigned int ARMword;    /* must be 32 bits wide */
typedef unsigned char ARMbyte;    /* must be 8 bits wide */
typedef unsigned short ARMhword;    /* must be 16 bits wide */
typedef struct ARMul_State ARMul_State;
typedef struct ARMul_io ARMul_io;
typedef struct ARMul_Energy ARMul_Energy;

//teawater add for arm2x86 2005.06.24-------------------------------------------
#include <stdint.h>
//AJ2D--------------------------------------------------------------------------
/*
//chy 2005-05-11
#ifndef __CYGWIN__
//teawater add for arm2x86 2005.02.14-------------------------------------------
typedef unsigned char           uint8_t;
typedef unsigned short          uint16_t;
typedef unsigned int            u32;
#if defined (__x86_64__)
typedef unsigned long           uint64_t;
#else
typedef unsigned long long      uint64_t;
#endif
////AJ2D--------------------------------------------------------------------------
#endif
*/

#include "armmmu.h"
//#include "lcd/skyeye_lcd.h"


//#include "skyeye.h"
//#include "skyeye_device.h"
//#include "net/skyeye_net.h"
//#include "skyeye_config.h"


typedef unsigned ARMul_CPInits (ARMul_State * state);
typedef unsigned ARMul_CPExits (ARMul_State * state);
typedef unsigned ARMul_LDCs (ARMul_State * state, unsigned type,
                 ARMword instr, ARMword value);
typedef unsigned ARMul_STCs (ARMul_State * state, unsigned type,
                 ARMword instr, ARMword * value);
typedef unsigned ARMul_MRCs (ARMul_State * state, unsigned type,
                 ARMword instr, ARMword * value);
typedef unsigned ARMul_MCRs (ARMul_State * state, unsigned type,
                 ARMword instr, ARMword value);
typedef unsigned ARMul_MRRCs (ARMul_State * state, unsigned type,
                 ARMword instr, ARMword * value1, ARMword * value2);
typedef unsigned ARMul_MCRRs (ARMul_State * state, unsigned type,
                 ARMword instr, ARMword value1, ARMword value2);
typedef unsigned ARMul_CDPs (ARMul_State * state, unsigned type,
                 ARMword instr);
typedef unsigned ARMul_CPReads (ARMul_State * state, unsigned reg,
                ARMword * value);
typedef unsigned ARMul_CPWrites (ARMul_State * state, unsigned reg,
                 ARMword value);


//added by ksh,2004-3-5
struct ARMul_io
{
    ARMword *instr;        //to display the current interrupt state
    ARMword *net_flag;    //to judge if network is enabled
    ARMword *net_int;    //netcard interrupt

    //ywc,2004-04-01
    ARMword *ts_int;
    ARMword *ts_is_enable;
    ARMword *ts_addr_begin;
    ARMword *ts_addr_end;
    ARMword *ts_buffer;
};

/* added by ksh,2004-11-26,some energy profiling */
struct ARMul_Energy
{
    int energy_prof;    /* <tktan>  BUG200103282109 : for energy profiling */
    int enable_func_energy;    /* <tktan> BUG200105181702 */
    char *func_energy;
    int func_display;    /* <tktan> BUG200103311509 : for function call display */
    int func_disp_start;    /* <tktan> BUG200104191428 : to start func profiling */
    char *start_func;    /* <tktan> BUG200104191428 */

    FILE *outfile;        /* <tktan> BUG200105201531 : direct console to file */
    long long tcycle, pcycle;
    float t_energy;
    void *cur_task;        /* <tktan> BUG200103291737 */
    long long t_mem_cycle, t_idle_cycle, t_uart_cycle;
    long long p_mem_cycle, p_idle_cycle, p_uart_cycle;
    long long p_io_update_tcycle;
    /*record CCCR,to get current core frequency */
    ARMword cccr;
};
#if 0
#define MAX_BANK 8
#define MAX_STR  1024

typedef struct mem_bank
{
    ARMword (*read_byte) (ARMul_State * state, ARMword addr);
    void (*write_byte) (ARMul_State * state, ARMword addr, ARMword data);
      ARMword (*read_halfword) (ARMul_State * state, ARMword addr);
    void (*write_halfword) (ARMul_State * state, ARMword addr,
                ARMword data);
      ARMword (*read_word) (ARMul_State * state, ARMword addr);
    void (*write_word) (ARMul_State * state, ARMword addr, ARMword data);
    unsigned int addr, len;
    char filename[MAX_STR];
    unsigned type;        //chy 2003-09-21: maybe io,ram,rom
} mem_bank_t;
typedef struct
{
    int bank_num;
    int current_num;    /*current num of bank */
    mem_bank_t mem_banks[MAX_BANK];
} mem_config_t;
#endif
#define VFP_REG_NUM 64
struct ARMul_State
{
    ARMword Emulate;    /* to start and stop emulation */
    unsigned EndCondition;    /* reason for stopping */
    unsigned ErrorCode;    /* type of illegal instruction */

    /* Order of the following register should not be modified */
    ARMword Reg[16];    /* the current register file */
    ARMword Cpsr;        /* the current psr */
    ARMword Spsr_copy;
    ARMword phys_pc;
    ARMword Reg_usr[2];
    ARMword Reg_svc[2]; /* R13_SVC R14_SVC */
    ARMword Reg_abort[2]; /* R13_ABORT R14_ABORT */
    ARMword Reg_undef[2]; /* R13 UNDEF R14 UNDEF */
    ARMword Reg_irq[2];   /* R13_IRQ R14_IRQ */
    ARMword Reg_firq[7];  /* R8---R14 FIRQ */
    ARMword Spsr[7];    /* the exception psr's */
    ARMword Mode;        /* the current mode */
    ARMword Bank;        /* the current register bank */
    ARMword exclusive_tag;
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
        //ARMword translate_pc;

    /* add armv6 flags dyf:2010-08-09 */
    ARMword GEFlag, EFlag, AFlag, QFlags;
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
    unsigned NextInstr;
    unsigned VectorCatch;    /* caught exception mask */
    unsigned CallDebug;    /* set to call the debugger */
    unsigned CanWatch;    /* set by memory interface if its willing to suffer the
                   overhead of checking for watchpoints on each memory
                   access */
    unsigned int StopHandle;

    char *CommandLine;    /* Command Line from ARMsd */

    ARMul_CPInits *CPInit[16];    /* coprocessor initialisers */
    ARMul_CPExits *CPExit[16];    /* coprocessor finalisers */
    ARMul_LDCs *LDC[16];    /* LDC instruction */
    ARMul_STCs *STC[16];    /* STC instruction */
    ARMul_MRCs *MRC[16];    /* MRC instruction */
    ARMul_MCRs *MCR[16];    /* MCR instruction */
    ARMul_MRRCs *MRRC[16];    /* MRRC instruction */
    ARMul_MCRRs *MCRR[16];    /* MCRR instruction */
    ARMul_CDPs *CDP[16];    /* CDP instruction */
    ARMul_CPReads *CPRead[16];    /* Read CP register */
    ARMul_CPWrites *CPWrite[16];    /* Write CP register */
    unsigned char *CPData[16];    /* Coprocessor data */
    unsigned char const *CPRegWords[16];    /* map of coprocessor register sizes */

    unsigned EventSet;    /* the number of events in the queue */
    unsigned int Now;    /* time to the nearest cycle */
    struct EventNode **EventPtr;    /* the event list */

    unsigned Debug;        /* show instructions as they are executed */
    unsigned NresetSig;    /* reset the processor */
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

    ARMword Vector;        /* synthesize aborts in cycle modes */
    ARMword Aborted;    /* sticky flag for aborts */
    ARMword Reseted;    /* sticky flag for Reset */
    ARMword Inted, LastInted;    /* sticky flags for interrupts */
    ARMword Base;        /* extra hand for base writeback */
    ARMword AbortAddr;    /* to keep track of Prefetch aborts */

    const struct Dbg_HostosInterface *hostif;

    int verbose;        /* non-zero means print various messages like the banner */

    mmu_state_t mmu;
    int mmu_inited;
    //mem_state_t mem;
    /*remove io_state to skyeye_mach_*.c files */
    //io_state_t io;
    /* point to a interrupt pending register. now for skyeye-ne2k.c
     * later should move somewhere. e.g machine_config_t*/


    //chy: 2003-08-11, for different arm core type
    unsigned is_v4;        /* Are we emulating a v4 architecture (or higher) ?  */
    unsigned is_v5;        /* Are we emulating a v5 architecture ?  */
    unsigned is_v5e;    /* Are we emulating a v5e architecture ?  */
    unsigned is_v6;        /* Are we emulating a v6 architecture ?  */
    unsigned is_v7;        /* Are we emulating a v7 architecture ?  */
    unsigned is_XScale;    /* Are we emulating an XScale architecture ?  */
    unsigned is_iWMMXt;    /* Are we emulating an iWMMXt co-processor ?  */
    unsigned is_ep9312;    /* Are we emulating a Cirrus Maverick co-processor ?  */
    //chy 2005-09-19
    unsigned is_pxa27x;    /* Are we emulating a Intel PXA27x co-processor ?  */
    //chy: seems only used in xscale's CP14
    unsigned int LastTime;    /* Value of last call to ARMul_Time() */
    ARMword CP14R0_CCD;    /* used to count 64 clock cycles with CP14 R0 bit 3 set */


//added by ksh:for handle different machs io 2004-3-5
    ARMul_io mach_io;

/*added by ksh,2004-11-26,some energy profiling*/
    ARMul_Energy energy;

//teawater add for next_dis 2004.10.27-----------------------
    int disassemble;
//AJ2D------------------------------------------

//teawater add for arm2x86 2005.02.15-------------------------------------------
    u32 trap;
    u32 tea_break_addr;
    u32 tea_break_ok;
    int tea_pc;
//AJ2D--------------------------------------------------------------------------
//teawater add for arm2x86 2005.07.03-------------------------------------------

    /*
     * 2007-01-24 removed the term-io functions by Anthony Lee,
     * moved to "device/uart/skyeye_uart_stdio.c".
     */

//AJ2D--------------------------------------------------------------------------
//teawater add for arm2x86 2005.07.05-------------------------------------------
    //arm_arm A2-18
    int abort_model;    //0 Base Restored Abort Model, 1 the Early Abort Model, 2 Base Updated Abort Model 
//AJ2D--------------------------------------------------------------------------
//teawater change for return if running tb dirty 2005.07.09---------------------
    void *tb_now;
//AJ2D--------------------------------------------------------------------------

//teawater add for record reg value to ./reg.txt 2005.07.10---------------------
    FILE *tea_reg_fd;
//AJ2D--------------------------------------------------------------------------

/*added by ksh in 2005-10-1*/
    cpu_config_t *cpu;
    //mem_config_t *mem_bank;

/* added LPC remap function */
    int vector_remap_flag;
    u32 vector_remap_addr;
    u32 vector_remap_size;

    u32 step;
    u32 cycle;
    int stop_simulator;
    conf_object_t *dyncom_cpu;
//teawater add DBCT_TEST_SPEED 2005.10.04---------------------------------------
#ifdef DBCT_TEST_SPEED
    uint64_t    instr_count;
#endif    //DBCT_TEST_SPEED
//    FILE * state_log;
//diff log
//#if DIFF_STATE
    FILE * state_log;
//#endif
    /* monitored memory for exclusice access */
    ARMword exclusive_tag_array[128];
    /* 1 means exclusive access and 0 means open access */
    ARMword exclusive_access_state;

    memory_space_intf space;    
    u32 CurrInstr;
    u32 last_pc; /* the last pc executed */
    u32 last_instr; /* the last inst executed */
    u32 WriteAddr[17];
    u32 WriteData[17];
    u32 WritePc[17];
    u32 CurrWrite;
};
#define DIFF_WRITE 0

typedef ARMul_State arm_core_t;
#define ResetPin NresetSig
#define FIQPin NfiqSig
#define IRQPin NirqSig
#define AbortPin abortSig
#define TransPin NtransSig
#define BigEndPin bigendSig
#define Prog32Pin prog32Sig
#define Data32Pin data32Sig
#define LateAbortPin lateabtSig

/***************************************************************************\
*                        Types of ARM we know about                         *
\***************************************************************************/

/* The bitflags */
#define ARM_Fix26_Prop   0x01
#define ARM_Nexec_Prop   0x02
#define ARM_Debug_Prop   0x10
#define ARM_Isync_Prop   ARM_Debug_Prop
#define ARM_Lock_Prop    0x20
//chy 2003-08-11 
#define ARM_v4_Prop      0x40
#define ARM_v5_Prop      0x80
/*jeff.du 2010-08-05 */
#define ARM_v6_Prop      0xc0

#define ARM_v5e_Prop     0x100
#define ARM_XScale_Prop  0x200
#define ARM_ep9312_Prop  0x400
#define ARM_iWMMXt_Prop  0x800
//chy 2005-09-19
#define ARM_PXA27X_Prop  0x1000
#define ARM_v7_Prop      0x2000

/* ARM2 family */
#define ARM2    (ARM_Fix26_Prop)
#define ARM2as  ARM2
#define ARM61   ARM2
#define ARM3    ARM2

#ifdef ARM60            /* previous definition in armopts.h */
#undef ARM60
#endif

/* ARM6 family */
#define ARM6    (ARM_Lock_Prop)
#define ARM60   ARM6
#define ARM600  ARM6
#define ARM610  ARM6
#define ARM620  ARM6


/***************************************************************************\
*                   Macros to extract instruction fields                    *
\***************************************************************************/

#define BIT(n) ( (ARMword)(instr>>(n))&1)    /* bit n of instruction */
#define BITS(m,n) ( (ARMword)(instr<<(31-(n))) >> ((31-(n))+(m)) )    /* bits m to n of instr */
#define TOPBITS(n) (instr >> (n))    /* bits 31 to n of instr */

/***************************************************************************\
*                      The hardware vector addresses                        *
\***************************************************************************/

#define ARMResetV 0L
#define ARMUndefinedInstrV 4L
#define ARMSWIV 8L
#define ARMPrefetchAbortV 12L
#define ARMDataAbortV 16L
#define ARMAddrExceptnV 20L
#define ARMIRQV 24L
#define ARMFIQV 28L
#define ARMErrorV 32L        /* This is an offset, not an address ! */

#define ARMul_ResetV ARMResetV
#define ARMul_UndefinedInstrV ARMUndefinedInstrV
#define ARMul_SWIV ARMSWIV
#define ARMul_PrefetchAbortV ARMPrefetchAbortV
#define ARMul_DataAbortV ARMDataAbortV
#define ARMul_AddrExceptnV ARMAddrExceptnV
#define ARMul_IRQV ARMIRQV
#define ARMul_FIQV ARMFIQV

/***************************************************************************\
*                          Mode and Bank Constants                          *
\***************************************************************************/

#define USER26MODE 0L
#define FIQ26MODE 1L
#define IRQ26MODE 2L
#define SVC26MODE 3L
#define USER32MODE 16L
#define FIQ32MODE 17L
#define IRQ32MODE 18L
#define SVC32MODE 19L
#define ABORT32MODE 23L
#define UNDEF32MODE 27L
//chy 2006-02-15 add system32 mode
#define SYSTEM32MODE 31L

#define ARM32BITMODE (state->Mode > 3)
#define ARM26BITMODE (state->Mode <= 3)
#define ARMMODE (state->Mode)
#define ARMul_MODEBITS 0x1fL
#define ARMul_MODE32BIT ARM32BITMODE
#define ARMul_MODE26BIT ARM26BITMODE

#define USERBANK 0
#define FIQBANK 1
#define IRQBANK 2
#define SVCBANK 3
#define ABORTBANK 4
#define UNDEFBANK 5
#define DUMMYBANK 6
#define SYSTEMBANK USERBANK
#define BANK_CAN_ACCESS_SPSR(bank)  \
  ((bank) != USERBANK && (bank) != SYSTEMBANK && (bank) != DUMMYBANK)


/***************************************************************************\
*                  Definitons of things in the emulator                     *
\***************************************************************************/
#ifdef __cplusplus
extern "C" {
#endif
extern void ARMul_EmulateInit (void);
extern void ARMul_Reset (ARMul_State * state);
#ifdef __cplusplus
    }
#endif
extern ARMul_State *ARMul_NewState (ARMul_State * state);
extern ARMword ARMul_DoProg (ARMul_State * state);
extern ARMword ARMul_DoInstr (ARMul_State * state);
/***************************************************************************\
*                Definitons of things for event handling                    *
\***************************************************************************/

extern void ARMul_ScheduleEvent (ARMul_State * state, unsigned int delay,
                 unsigned (*func) ());
extern void ARMul_EnvokeEvent (ARMul_State * state);
extern unsigned int ARMul_Time (ARMul_State * state);

/***************************************************************************\
*                          Useful support routines                          *
\***************************************************************************/

extern ARMword ARMul_GetReg (ARMul_State * state, unsigned mode,
                 unsigned reg);
extern void ARMul_SetReg (ARMul_State * state, unsigned mode, unsigned reg,
              ARMword value);
extern ARMword ARMul_GetPC (ARMul_State * state);
extern ARMword ARMul_GetNextPC (ARMul_State * state);
extern void ARMul_SetPC (ARMul_State * state, ARMword value);
extern ARMword ARMul_GetR15 (ARMul_State * state);
extern void ARMul_SetR15 (ARMul_State * state, ARMword value);

extern ARMword ARMul_GetCPSR (ARMul_State * state);
extern void ARMul_SetCPSR (ARMul_State * state, ARMword value);
extern ARMword ARMul_GetSPSR (ARMul_State * state, ARMword mode);
extern void ARMul_SetSPSR (ARMul_State * state, ARMword mode, ARMword value);

/***************************************************************************\
*                  Definitons of things to handle aborts                    *
\***************************************************************************/

extern void ARMul_Abort (ARMul_State * state, ARMword address);
#ifdef MODET
#define ARMul_ABORTWORD (state->TFlag ? 0xefffdfff : 0xefffffff)    /* SWI -1 */
#define ARMul_PREFETCHABORT(address) if (state->AbortAddr == 1) \
                                        state->AbortAddr = (address & (state->TFlag ? ~1L : ~3L))
#else
#define ARMul_ABORTWORD 0xefffffff    /* SWI -1 */
#define ARMul_PREFETCHABORT(address) if (state->AbortAddr == 1) \
                                        state->AbortAddr = (address & ~3L)
#endif
#define ARMul_DATAABORT(address) state->abortSig = HIGH ; \
                                 state->Aborted = ARMul_DataAbortV ;
#define ARMul_CLEARABORT state->abortSig = LOW

/***************************************************************************\
*              Definitons of things in the memory interface                 *
\***************************************************************************/

extern unsigned ARMul_MemoryInit (ARMul_State * state,
                  unsigned int initmemsize);
extern void ARMul_MemoryExit (ARMul_State * state);

extern ARMword ARMul_LoadInstrS (ARMul_State * state, ARMword address,
                 ARMword isize);
extern ARMword ARMul_LoadInstrN (ARMul_State * state, ARMword address,
                 ARMword isize);
#ifdef __cplusplus
extern "C" {
#endif
extern ARMword ARMul_ReLoadInstr (ARMul_State * state, ARMword address,
                  ARMword isize);
#ifdef __cplusplus
    }
#endif
extern ARMword ARMul_LoadWordS (ARMul_State * state, ARMword address);
extern ARMword ARMul_LoadWordN (ARMul_State * state, ARMword address);
extern ARMword ARMul_LoadHalfWord (ARMul_State * state, ARMword address);
extern ARMword ARMul_LoadByte (ARMul_State * state, ARMword address);

extern void ARMul_StoreWordS (ARMul_State * state, ARMword address,
                  ARMword data);
extern void ARMul_StoreWordN (ARMul_State * state, ARMword address,
                  ARMword data);
extern void ARMul_StoreHalfWord (ARMul_State * state, ARMword address,
                 ARMword data);
extern void ARMul_StoreByte (ARMul_State * state, ARMword address,
                 ARMword data);

extern ARMword ARMul_SwapWord (ARMul_State * state, ARMword address,
                   ARMword data);
extern ARMword ARMul_SwapByte (ARMul_State * state, ARMword address,
                   ARMword data);

extern void ARMul_Icycles (ARMul_State * state, unsigned number,
               ARMword address);
extern void ARMul_Ccycles (ARMul_State * state, unsigned number,
               ARMword address);

extern ARMword ARMul_ReadWord (ARMul_State * state, ARMword address);
extern ARMword ARMul_ReadByte (ARMul_State * state, ARMword address);
extern void ARMul_WriteWord (ARMul_State * state, ARMword address,
                 ARMword data);
extern void ARMul_WriteByte (ARMul_State * state, ARMword address,
                 ARMword data);

extern ARMword ARMul_MemAccess (ARMul_State * state, ARMword, ARMword,
                ARMword, ARMword, ARMword, ARMword, ARMword,
                ARMword, ARMword, ARMword);

/***************************************************************************\
*            Definitons of things in the co-processor interface             *
\***************************************************************************/

#define ARMul_FIRST 0
#define ARMul_TRANSFER 1
#define ARMul_BUSY 2
#define ARMul_DATA 3
#define ARMul_INTERRUPT 4
#define ARMul_DONE 0
#define ARMul_CANT 1
#define ARMul_INC 3

#define ARMul_CP13_R0_FIQ       0x1
#define ARMul_CP13_R0_IRQ       0x2
#define ARMul_CP13_R8_PMUS      0x1

#define ARMul_CP14_R0_ENABLE    0x0001
#define ARMul_CP14_R0_CLKRST    0x0004
#define ARMul_CP14_R0_CCD       0x0008
#define ARMul_CP14_R0_INTEN0    0x0010
#define ARMul_CP14_R0_INTEN1    0x0020
#define ARMul_CP14_R0_INTEN2    0x0040
#define ARMul_CP14_R0_FLAG0     0x0100
#define ARMul_CP14_R0_FLAG1     0x0200
#define ARMul_CP14_R0_FLAG2     0x0400
#define ARMul_CP14_R10_MOE_IB   0x0004
#define ARMul_CP14_R10_MOE_DB   0x0008
#define ARMul_CP14_R10_MOE_BT   0x000c
#define ARMul_CP15_R1_ENDIAN    0x0080
#define ARMul_CP15_R1_ALIGN     0x0002
#define ARMul_CP15_R5_X         0x0400
#define ARMul_CP15_R5_ST_ALIGN  0x0001
#define ARMul_CP15_R5_IMPRE     0x0406
#define ARMul_CP15_R5_MMU_EXCPT 0x0400
#define ARMul_CP15_DBCON_M      0x0100
#define ARMul_CP15_DBCON_E1     0x000c
#define ARMul_CP15_DBCON_E0     0x0003

extern unsigned ARMul_CoProInit (ARMul_State * state);
extern void ARMul_CoProExit (ARMul_State * state);
extern void ARMul_CoProAttach (ARMul_State * state, unsigned number,
                   ARMul_CPInits * init, ARMul_CPExits * exit,
                   ARMul_LDCs * ldc, ARMul_STCs * stc,
                   ARMul_MRCs * mrc, ARMul_MCRs * mcr,
                   ARMul_MRRCs * mrrc, ARMul_MCRRs * mcrr,
                   ARMul_CDPs * cdp,
                   ARMul_CPReads * read, ARMul_CPWrites * write);
extern void ARMul_CoProDetach (ARMul_State * state, unsigned number);

/***************************************************************************\
*               Definitons of things in the host environment                *
\***************************************************************************/

extern unsigned ARMul_OSInit (ARMul_State * state);
extern void ARMul_OSExit (ARMul_State * state);

#ifdef __cplusplus
 extern "C" {
#endif

extern unsigned ARMul_OSHandleSWI (ARMul_State * state, ARMword number);
#ifdef __cplusplus
}
#endif


extern ARMword ARMul_OSLastErrorP (ARMul_State * state);

extern ARMword ARMul_Debug (ARMul_State * state, ARMword pc, ARMword instr);
extern unsigned ARMul_OSException (ARMul_State * state, ARMword vector,
                   ARMword pc);
extern int rdi_log;

/***************************************************************************\
*                            Host-dependent stuff                           *
\***************************************************************************/

#ifdef macintosh
pascal void SpinCursor (short increment);    /* copied from CursorCtl.h */
# define HOURGLASS           SpinCursor( 1 )
# define HOURGLASS_RATE      1023    /* 2^n - 1 */
#endif

//teawater add for arm2x86 2005.02.14-------------------------------------------
/*ywc 2005-03-31*/
/*
#include "arm2x86.h"
#include "arm2x86_dp.h"
#include "arm2x86_movl.h"
#include "arm2x86_psr.h"
#include "arm2x86_shift.h"
#include "arm2x86_mem.h"
#include "arm2x86_mul.h"
#include "arm2x86_test.h"
#include "arm2x86_other.h"
#include "list.h"
#include "tb.h"
*/
#define EQ 0
#define NE 1
#define CS 2
#define CC 3
#define MI 4
#define PL 5
#define VS 6
#define VC 7
#define HI 8
#define LS 9
#define GE 10
#define LT 11
#define GT 12
#define LE 13
#define AL 14
#define NV 15

#ifndef NFLAG
#define NFLAG    state->NFlag
#endif //NFLAG

#ifndef ZFLAG
#define ZFLAG    state->ZFlag
#endif //ZFLAG

#ifndef CFLAG
#define CFLAG    state->CFlag
#endif //CFLAG

#ifndef VFLAG
#define VFLAG    state->VFlag
#endif //VFLAG

#ifndef IFLAG
#define IFLAG    (state->IFFlags >> 1)
#endif //IFLAG

#ifndef FFLAG
#define FFLAG    (state->IFFlags & 1)
#endif //FFLAG

#ifndef IFFLAGS
#define IFFLAGS    state->IFFlags
#endif //VFLAG

#define FLAG_MASK    0xf0000000
#define NBIT_SHIFT    31
#define ZBIT_SHIFT    30
#define CBIT_SHIFT    29
#define VBIT_SHIFT    28
#ifdef DBCT
//teawater change for local tb branch directly jump 2005.10.18------------------
#include "dbct/list.h"
#include "dbct/arm2x86.h"
#include "dbct/arm2x86_dp.h"
#include "dbct/arm2x86_movl.h"
#include "dbct/arm2x86_psr.h"
#include "dbct/arm2x86_shift.h"
#include "dbct/arm2x86_mem.h"
#include "dbct/arm2x86_mul.h"
#include "dbct/arm2x86_test.h"
#include "dbct/arm2x86_other.h"
#include "dbct/arm2x86_coproc.h"
#include "dbct/tb.h"
#endif
//AJ2D--------------------------------------------------------------------------
//AJ2D--------------------------------------------------------------------------
#define SKYEYE_OUTREGS(fd) { fprintf ((fd), "R %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,C %x,S %x,%x,%x,%x,%x,%x,%x,M %x,B %x,E %x,I %x,P %x,T %x,L %x,D %x,",\
                         state->Reg[0],state->Reg[1],state->Reg[2],state->Reg[3], \
                         state->Reg[4],state->Reg[5],state->Reg[6],state->Reg[7], \
                         state->Reg[8],state->Reg[9],state->Reg[10],state->Reg[11], \
                         state->Reg[12],state->Reg[13],state->Reg[14],state->Reg[15], \
             state->Cpsr,   state->Spsr[0], state->Spsr[1], state->Spsr[2],\
                         state->Spsr[3],state->Spsr[4], state->Spsr[5], state->Spsr[6],\
             state->Mode,state->Bank,state->ErrorCode,state->instr,state->pc,\
             state->temp,state->loaded,state->decoded);}

#define SKYEYE_OUTMOREREGS(fd) { fprintf ((fd),"\
RUs %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,\
RF  %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,\
RI  %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,\
RS  %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,\
RA  %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,\
RUn %x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x,%x\n",\
                         state->RegBank[0][0],state->RegBank[0][1],state->RegBank[0][2],state->RegBank[0][3], \
                         state->RegBank[0][4],state->RegBank[0][5],state->RegBank[0][6],state->RegBank[0][7], \
                         state->RegBank[0][8],state->RegBank[0][9],state->RegBank[0][10],state->RegBank[0][11], \
                         state->RegBank[0][12],state->RegBank[0][13],state->RegBank[0][14],state->RegBank[0][15], \
                         state->RegBank[1][0],state->RegBank[1][1],state->RegBank[1][2],state->RegBank[1][3], \
                         state->RegBank[1][4],state->RegBank[1][5],state->RegBank[1][6],state->RegBank[1][7], \
                         state->RegBank[1][8],state->RegBank[1][9],state->RegBank[1][10],state->RegBank[1][11], \
                         state->RegBank[1][12],state->RegBank[1][13],state->RegBank[1][14],state->RegBank[1][15], \
                         state->RegBank[2][0],state->RegBank[2][1],state->RegBank[2][2],state->RegBank[2][3], \
                         state->RegBank[2][4],state->RegBank[2][5],state->RegBank[2][6],state->RegBank[2][7], \
                         state->RegBank[2][8],state->RegBank[2][9],state->RegBank[2][10],state->RegBank[2][11], \
                         state->RegBank[2][12],state->RegBank[2][13],state->RegBank[2][14],state->RegBank[2][15], \
                         state->RegBank[3][0],state->RegBank[3][1],state->RegBank[3][2],state->RegBank[3][3], \
                         state->RegBank[3][4],state->RegBank[3][5],state->RegBank[3][6],state->RegBank[3][7], \
                         state->RegBank[3][8],state->RegBank[3][9],state->RegBank[3][10],state->RegBank[3][11], \
                         state->RegBank[3][12],state->RegBank[3][13],state->RegBank[3][14],state->RegBank[3][15], \
                         state->RegBank[4][0],state->RegBank[4][1],state->RegBank[4][2],state->RegBank[4][3], \
                         state->RegBank[4][4],state->RegBank[4][5],state->RegBank[4][6],state->RegBank[4][7], \
                         state->RegBank[4][8],state->RegBank[4][9],state->RegBank[4][10],state->RegBank[4][11], \
                         state->RegBank[4][12],state->RegBank[4][13],state->RegBank[4][14],state->RegBank[4][15], \
                         state->RegBank[5][0],state->RegBank[5][1],state->RegBank[5][2],state->RegBank[5][3], \
                         state->RegBank[5][4],state->RegBank[5][5],state->RegBank[5][6],state->RegBank[5][7], \
                         state->RegBank[5][8],state->RegBank[5][9],state->RegBank[5][10],state->RegBank[5][11], \
                         state->RegBank[5][12],state->RegBank[5][13],state->RegBank[5][14],state->RegBank[5][15] \
        );}


#define SA1110        0x6901b110
#define SA1100        0x4401a100
#define PXA250          0x69052100
#define PXA270           0x69054110
//#define PXA250              0x69052903
// 0x69052903;  //PXA250 B1 from intel 278522-001.pdf


extern void ARMul_UndefInstr (ARMul_State *, ARMword);
extern void ARMul_FixCPSR (ARMul_State *, ARMword, ARMword);
extern void ARMul_FixSPSR (ARMul_State *, ARMword, ARMword);
extern void ARMul_ConsolePrint (ARMul_State *, const char *, ...);
extern void ARMul_SelectProcessor (ARMul_State *, unsigned);

#define DIFF_LOG 0
#define SAVE_LOG 0

#endif /* _ARMDEFS_H_ */
