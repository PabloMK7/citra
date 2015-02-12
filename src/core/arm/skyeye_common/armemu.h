/*  armemu.h -- ARMulator emulation macros:  ARM6 Instruction Emulator.
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

#include "core/arm/skyeye_common/armdefs.h"

/* Macros to twiddle the status flags and mode.  */
#define NBIT ((unsigned)1L << 31)
#define ZBIT (1L << 30)
#define CBIT (1L << 29)
#define VBIT (1L << 28)
#define QBIT (1L << 27)
#define IBIT (1L << 7)
#define FBIT (1L << 6)
#define IFBITS (3L << 6)
#define R15IBIT (1L << 27)
#define R15FBIT (1L << 26)
#define R15IFBITS (3L << 26)

#if defined MODE32 || defined MODET
#define CCBITS (0xf8000000L)
#else
#define CCBITS (0xf0000000L)
#endif

#define INTBITS (0xc0L)

#if defined MODET && defined MODE32
#define PCBITS (0xffffffffL)
#else
#define PCBITS (0xfffffffcL)
#endif

#define MODEBITS (0x1fL)
#define R15INTBITS (3L << 26)

#if defined MODET && defined MODE32
#define R15PCBITS (0x03ffffffL)
#else
#define R15PCBITS (0x03fffffcL)
#endif

#define R15MODEBITS (0x3L)

#ifdef MODE32
#define PCMASK PCBITS
#define PCWRAP(pc) (pc)
#else
#define PCMASK R15PCBITS
#define PCWRAP(pc) ((pc) & R15PCBITS)
#endif

#define PC (state->Reg[15] & PCMASK)
#define R15CCINTMODE (state->Reg[15] & (CCBITS | R15INTBITS | R15MODEBITS))
#define R15INT (state->Reg[15] & R15INTBITS)
#define R15INTPC (state->Reg[15] & (R15INTBITS | R15PCBITS))
#define R15INTPCMODE (state->Reg[15] & (R15INTBITS | R15PCBITS | R15MODEBITS))
#define R15INTMODE (state->Reg[15] & (R15INTBITS | R15MODEBITS))
#define R15PC (state->Reg[15] & R15PCBITS)
#define R15PCMODE (state->Reg[15] & (R15PCBITS | R15MODEBITS))
#define R15MODE (state->Reg[15] & R15MODEBITS)

// Different ways to start the next instruction.
enum {
    SEQ           = 0,
    NONSEQ        = 1,
    PCINCEDSEQ    = 2,
    PCINCEDNONSEQ = 3,
    PRIMEPIPE     = 4,
    RESUME        = 8
};

// Values for Emulate.
enum {
    STOP       = 0, // Stop
    CHANGEMODE = 1, // Change mode
    ONCE       = 2, // Execute just one interation
    RUN        = 3  // Continuous execution
};

#define FLUSHPIPE state->NextInstr |= PRIMEPIPE

// Coprocessor support functions.
extern void ARMul_CoProInit(ARMul_State*);
extern void ARMul_CoProExit(ARMul_State*);
extern void ARMul_CoProAttach(ARMul_State*, unsigned, ARMul_CPInits*,
                              ARMul_CPExits*, ARMul_LDCs*, ARMul_STCs*,
                              ARMul_MRCs*, ARMul_MCRs*, ARMul_MRRCs*, ARMul_MCRRs*,
                              ARMul_CDPs*, ARMul_CPReads*, ARMul_CPWrites*);
extern void ARMul_CoProDetach(ARMul_State*, unsigned);
