/*  armcopro.c -- co-processor interface:  ARM6 Instruction Emulator.
    Copyright (C) 1994, 2000 Advanced RISC Machines Ltd.
 
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
    Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.  */


#include "core/arm/interpreter/armdefs.h"
#include "core/arm/interpreter/armos.h"
#include "core/arm/interpreter/armemu.h"
#include "core/arm/interpreter/vfp/vfp.h"

//chy 2005-07-08
//#include "ansidecl.h"
//chy -------
//#include "iwmmxt.h"


//chy 2005-09-19 add CP6 MRC support (for get irq number and base)
extern unsigned xscale_cp6_mrc (ARMul_State * state, unsigned type,
				ARMword instr, ARMword * data);
//chy 2005-09-19---------------

extern unsigned xscale_cp13_init (ARMul_State * state);
extern unsigned xscale_cp13_exit (ARMul_State * state);
extern unsigned xscale_cp13_ldc (ARMul_State * state, unsigned type,
				 ARMword instr, ARMword data);
extern unsigned xscale_cp13_stc (ARMul_State * state, unsigned type,
				 ARMword instr, ARMword * data);
extern unsigned xscale_cp13_mrc (ARMul_State * state, unsigned type,
				 ARMword instr, ARMword * data);
extern unsigned xscale_cp13_mcr (ARMul_State * state, unsigned type,
				 ARMword instr, ARMword data);
extern unsigned xscale_cp13_cdp (ARMul_State * state, unsigned type,
				 ARMword instr);
extern unsigned xscale_cp13_read_reg (ARMul_State * state, unsigned reg,
				      ARMword * data);
extern unsigned xscale_cp13_write_reg (ARMul_State * state, unsigned reg,
				       ARMword data);
extern unsigned xscale_cp14_init (ARMul_State * state);
extern unsigned xscale_cp14_exit (ARMul_State * state);
extern unsigned xscale_cp14_ldc (ARMul_State * state, unsigned type,
				 ARMword instr, ARMword data);
extern unsigned xscale_cp14_stc (ARMul_State * state, unsigned type,
				 ARMword instr, ARMword * data);
extern unsigned xscale_cp14_mrc (ARMul_State * state, unsigned type,
				 ARMword instr, ARMword * data);
extern unsigned xscale_cp14_mcr (ARMul_State * state, unsigned type,
				 ARMword instr, ARMword data);
extern unsigned xscale_cp14_cdp (ARMul_State * state, unsigned type,
				 ARMword instr);
extern unsigned xscale_cp14_read_reg (ARMul_State * state, unsigned reg,
				      ARMword * data);
extern unsigned xscale_cp14_write_reg (ARMul_State * state, unsigned reg,
				       ARMword data);
extern unsigned xscale_cp15_init (ARMul_State * state);
extern unsigned xscale_cp15_exit (ARMul_State * state);
extern unsigned xscale_cp15_ldc (ARMul_State * state, unsigned type,
				 ARMword instr, ARMword data);
extern unsigned xscale_cp15_stc (ARMul_State * state, unsigned type,
				 ARMword instr, ARMword * data);
extern unsigned xscale_cp15_mrc (ARMul_State * state, unsigned type,
				 ARMword instr, ARMword * data);
extern unsigned xscale_cp15_mcr (ARMul_State * state, unsigned type,
				 ARMword instr, ARMword data);
extern unsigned xscale_cp15_cdp (ARMul_State * state, unsigned type,
				 ARMword instr);
extern unsigned xscale_cp15_read_reg (ARMul_State * state, unsigned reg,
				      ARMword * data);
extern unsigned xscale_cp15_write_reg (ARMul_State * state, unsigned reg,
				       ARMword data);
extern unsigned xscale_cp15_cp_access_allowed (ARMul_State * state, unsigned reg,
					unsigned cpnum);

/* Dummy Co-processors.  */

static unsigned
NoCoPro3R (ARMul_State * state,
	   unsigned a, ARMword b)
{
	return ARMul_CANT;
}

static unsigned
NoCoPro4R (ARMul_State * state,
	   unsigned a,
	   ARMword b, ARMword c)
{
	return ARMul_CANT;
}

static unsigned
NoCoPro4W (ARMul_State * state,
	   unsigned a,
	   ARMword b, ARMword * c)
{
	return ARMul_CANT;
}

static unsigned
NoCoPro5R (ARMul_State * state,
	   unsigned a,
	   ARMword b, 
	   ARMword c, ARMword d)
{
	return ARMul_CANT;
}

static unsigned
NoCoPro5W (ARMul_State * state,
	   unsigned a,
	   ARMword b,
	   ARMword * c, ARMword * d )
{
	return ARMul_CANT;
}

/* The XScale Co-processors.  */

/* Coprocessor 15:  System Control.  */
static void write_cp14_reg (unsigned, ARMword);
static ARMword read_cp14_reg (unsigned);

/* There are two sets of registers for copro 15.
   One set is available when opcode_2 is 0 and
   the other set when opcode_2 >= 1.  */
static ARMword XScale_cp15_opcode_2_is_0_Regs[16];
static ARMword XScale_cp15_opcode_2_is_not_0_Regs[16];
/* There are also a set of breakpoint registers
   which are accessed via CRm instead of opcode_2.  */
static ARMword XScale_cp15_DBR1;
static ARMword XScale_cp15_DBCON;
static ARMword XScale_cp15_IBCR0;
static ARMword XScale_cp15_IBCR1;

static unsigned
XScale_cp15_init (ARMul_State * state)
{
	int i;

	for (i = 16; i--;) {
		XScale_cp15_opcode_2_is_0_Regs[i] = 0;
		XScale_cp15_opcode_2_is_not_0_Regs[i] = 0;
	}

	/* Initialise the processor ID.  */
	//chy 2003-03-24, is same as cpu id in skyeye_options.c
	//XScale_cp15_opcode_2_is_0_Regs[0] = 0x69052000;
	XScale_cp15_opcode_2_is_0_Regs[0] = 0x69050000;

	/* Initialise the cache type.  */
	XScale_cp15_opcode_2_is_not_0_Regs[0] = 0x0B1AA1AA;

	/* Initialise the ARM Control Register.  */
	XScale_cp15_opcode_2_is_0_Regs[1] = 0x00000078;

	return No_exp;
}

/* Check an access to a register.  */

static unsigned
check_cp15_access (ARMul_State * state,
		   unsigned reg,
		   unsigned CRm, unsigned opcode_1, unsigned opcode_2)
{
	/* Do not allow access to these register in USER mode.  */
	//chy 2006-02-16 , should not consider system mode, don't conside 26bit mode
	if (state->Mode == USER26MODE || state->Mode == USER32MODE )
		return ARMul_CANT;

	/* Opcode_1should be zero.  */
	if (opcode_1 != 0)
		return ARMul_CANT;

	/* Different register have different access requirements.  */
	switch (reg) {
	case 0:
	case 1:
		/* CRm must be 0.  Opcode_2 can be anything.  */
		if (CRm != 0)
			return ARMul_CANT;
		break;
	case 2:
	case 3:
		/* CRm must be 0.  Opcode_2 must be zero.  */
		if ((CRm != 0) || (opcode_2 != 0))
			return ARMul_CANT;
		break;
	case 4:
		/* Access not allowed.  */
		return ARMul_CANT;
	case 5:
	case 6:
		/* Opcode_2 must be zero.  CRm must be 0.  */
		if ((CRm != 0) || (opcode_2 != 0))
			return ARMul_CANT;
		break;
	case 7:
		/* Permissable combinations:
		   Opcode_2  CRm
		   0       5
		   0       6
		   0       7
		   1       5
		   1       6
		   1      10
		   4      10
		   5       2
		   6       5  */
		switch (opcode_2) {
		default:
			return ARMul_CANT;
		case 6:
			if (CRm != 5)
				return ARMul_CANT;
			break;
		case 5:
			if (CRm != 2)
				return ARMul_CANT;
			break;
		case 4:
			if (CRm != 10)
				return ARMul_CANT;
			break;
		case 1:
			if ((CRm != 5) && (CRm != 6) && (CRm != 10))
				return ARMul_CANT;
			break;
		case 0:
			if ((CRm < 5) || (CRm > 7))
				return ARMul_CANT;
			break;
		}
		break;

	case 8:
		/* Permissable combinations:
		   Opcode_2  CRm
		   0       5
		   0       6
		   0       7
		   1       5
		   1       6  */
		if (opcode_2 > 1)
			return ARMul_CANT;
		if ((CRm < 5) || (CRm > 7))
			return ARMul_CANT;
		if (opcode_2 == 1 && CRm == 7)
			return ARMul_CANT;
		break;
	case 9:
		/* Opcode_2 must be zero or one.  CRm must be 1 or 2.  */
		if (((CRm != 0) && (CRm != 1))
		    || ((opcode_2 != 1) && (opcode_2 != 2)))
			return ARMul_CANT;
		break;
	case 10:
		/* Opcode_2 must be zero or one.  CRm must be 4 or 8.  */
		if (((CRm != 0) && (CRm != 1))
		    || ((opcode_2 != 4) && (opcode_2 != 8)))
			return ARMul_CANT;
		break;
	case 11:
		/* Access not allowed.  */
		return ARMul_CANT;
	case 12:
		/* Access not allowed.  */
		return ARMul_CANT;
	case 13:
		/* Opcode_2 must be zero.  CRm must be 0.  */
		if ((CRm != 0) || (opcode_2 != 0))
			return ARMul_CANT;
		break;
	case 14:
		/* Opcode_2 must be 0.  CRm must be 0, 3, 4, 8 or 9.  */
		if (opcode_2 != 0)
			return ARMul_CANT;

		if ((CRm != 0) && (CRm != 3) && (CRm != 4) && (CRm != 8)
		    && (CRm != 9))
			return ARMul_CANT;
		break;
	case 15:
		/* Opcode_2 must be zero.  CRm must be 1.  */
		if ((CRm != 1) || (opcode_2 != 0))
			return ARMul_CANT;
		break;
	default:
		/* Should never happen.  */
		return ARMul_CANT;
	}

	return ARMul_DONE;
}

/* Coprocessor 13:  Interrupt Controller and Bus Controller.  */

/* There are two sets of registers for copro 13.
   One set (of three registers) is available when CRm is 0
   and the other set (of six registers) when CRm is 1.  */

static ARMword XScale_cp13_CR0_Regs[16];
static ARMword XScale_cp13_CR1_Regs[16];

static unsigned
XScale_cp13_init (ARMul_State * state)
{
	int i;

	for (i = 16; i--;) {
		XScale_cp13_CR0_Regs[i] = 0;
		XScale_cp13_CR1_Regs[i] = 0;
	}

	return No_exp;
}

/* Check an access to a register.  */

static unsigned
check_cp13_access (ARMul_State * state,
		   unsigned reg,
		   unsigned CRm, unsigned opcode_1, unsigned opcode_2)
{
	/* Do not allow access to these registers in USER mode.  */
	//chy 2006-02-16 , should not consider system mode, don't conside 26bit mode
	if (state->Mode == USER26MODE || state->Mode == USER32MODE )
		return ARMul_CANT;

	/* The opcodes should be zero.  */
	if ((opcode_1 != 0) || (opcode_2 != 0))
		return ARMul_CANT;

	/* Do not allow access to these register if bit
	   13 of coprocessor 15's register 15 is zero.  */
	if (!CP_ACCESS_ALLOWED (state, 13))
		return ARMul_CANT;

	/* Registers 0, 4 and 8 are defined when CRm == 0.
	   Registers 0, 1, 4, 5, 6, 7, 8 are defined when CRm == 1.
	   For all other CRm values undefined behaviour results.  */
	if (CRm == 0) {
		if (reg == 0 || reg == 4 || reg == 8)
			return ARMul_DONE;
	}
	else if (CRm == 1) {
		if (reg == 0 || reg == 1 || (reg >= 4 && reg <= 8))
			return ARMul_DONE;
	}

	return ARMul_CANT;
}

/* Coprocessor 14:  Performance Monitoring,  Clock and Power management,
   Software Debug.  */

static ARMword XScale_cp14_Regs[16];

static unsigned
XScale_cp14_init (ARMul_State * state)
{
	int i;

	for (i = 16; i--;)
		XScale_cp14_Regs[i] = 0;

	return No_exp;
}

/* Check an access to a register.  */

static unsigned
check_cp14_access (ARMul_State * state,
		   unsigned reg,
		   unsigned CRm, unsigned opcode1, unsigned opcode2)
{
	/* Not allowed to access these register in USER mode.  */
	//chy 2006-02-16 , should not consider system mode, don't conside 26bit mode
	if (state->Mode == USER26MODE || state->Mode == USER32MODE )
		return ARMul_CANT;

	/* CRm should be zero.  */
	if (CRm != 0)
		return ARMul_CANT;

	/* OPcodes should be zero.  */
	if (opcode1 != 0 || opcode2 != 0)
		return ARMul_CANT;

	/* Accessing registers 4 or 5 has unpredicatable results.  */
	if (reg >= 4 && reg <= 5)
		return ARMul_CANT;

	return ARMul_DONE;
}

/* Here's ARMulator's MMU definition.  A few things to note:
   1) It has eight registers, but only two are defined.
   2) You can only access its registers with MCR and MRC.
   3) MMU Register 0 (ID) returns 0x41440110
   4) Register 1 only has 4 bits defined.  Bits 0 to 3 are unused, bit 4
      controls 32/26 bit program space, bit 5 controls 32/26 bit data space,
      bit 6 controls late abort timimg and bit 7 controls big/little endian.  */

static ARMword MMUReg[8];

static unsigned
MMUInit (ARMul_State * state)
{
/* 2004-05-09 chy
-------------------------------------------------------------
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
because the ARMs which skyeye simulates are all belonged to  ARMv4,
so I think MMUReg[1]'s bit 6 should always be 1

*/

	MMUReg[1] = state->prog32Sig << 4 |
		state->data32Sig << 5 | 1 << 6 | state->bigendSig << 7;
	//state->data32Sig << 5 | state->lateabtSig << 6 | state->bigendSig << 7;


	NOTICE_LOG(ARM11, "ARMul_ConsolePrint: MMU present");

	return TRUE;
}

static unsigned
MMUMRC (ARMul_State * state, unsigned type,
	ARMword instr, ARMword * value)
{
	mmu_mrc (state, instr, value);
	return (ARMul_DONE);
}

static unsigned
MMUMCR (ARMul_State * state, unsigned type, ARMword instr, ARMword value)
{
	mmu_mcr (state, instr, value);
	return (ARMul_DONE);
}

/* What follows is the Validation Suite Coprocessor.  It uses two
   co-processor numbers (4 and 5) and has the follwing functionality.
   Sixteen registers.  Both co-processor nuimbers can be used in an MCR
   and MRC to access these registers.  CP 4 can LDC and STC to and from
   the registers.  CP 4 and CP 5 CDP 0 will busy wait for the number of
   cycles specified by a CP register.  CP 5 CDP 1 issues a FIQ after a
   number of cycles (specified in a CP register), CDP 2 issues an IRQW
   in the same way, CDP 3 and 4 turn of the FIQ and IRQ source, and CDP 5
   stores a 32 bit time value in a CP register (actually it's the total
   number of N, S, I, C and F cyles).  */

static ARMword ValReg[16];

static unsigned
ValLDC (ARMul_State * state,
	unsigned type, ARMword instr, ARMword data)
{
	static unsigned words;

	if (type != ARMul_DATA)
		words = 0;
	else {
		ValReg[BITS (12, 15)] = data;

		if (BIT (22))
			/* It's a long access, get two words.  */
			if (words++ != 4)
				return ARMul_INC;
	}

	return ARMul_DONE;
}

static unsigned
ValSTC (ARMul_State * state,
	unsigned type, ARMword instr, ARMword * data)
{
	static unsigned words;

	if (type != ARMul_DATA)
		words = 0;
	else {
		*data = ValReg[BITS (12, 15)];

		if (BIT (22))
			/* It's a long access, get two words.  */
			if (words++ != 4)
				return ARMul_INC;
	}

	return ARMul_DONE;
}

static unsigned
ValMRC (ARMul_State * state,
	unsigned type, ARMword instr, ARMword * value)
{
	*value = ValReg[BITS (16, 19)];

	return ARMul_DONE;
}

static unsigned
ValMCR (ARMul_State * state,
	unsigned type, ARMword instr, ARMword value)
{
	ValReg[BITS (16, 19)] = value;

	return ARMul_DONE;
}

static unsigned
ValCDP (ARMul_State * state, unsigned type, ARMword instr)
{
	static unsigned int finish = 0;

	if (BITS (20, 23) != 0)
		return ARMul_CANT;

	if (type == ARMul_FIRST) {
		ARMword howlong;

		howlong = ValReg[BITS (0, 3)];

		/* First cycle of a busy wait.  */
		finish = ARMul_Time (state) + howlong;

		return howlong == 0 ? ARMul_DONE : ARMul_BUSY;
	}
	else if (type == ARMul_BUSY) {
		if (ARMul_Time (state) >= finish)
			return ARMul_DONE;
		else
			return ARMul_BUSY;
	}

	return ARMul_CANT;
}

static unsigned
DoAFIQ (ARMul_State * state)
{
	state->NfiqSig = LOW;
	return 0;
}

static unsigned
DoAIRQ (ARMul_State * state)
{
	state->NirqSig = LOW;
	return 0;
}

static unsigned
IntCDP (ARMul_State * state, unsigned type, ARMword instr)
{
	static unsigned int finish;
	ARMword howlong;

	howlong = ValReg[BITS (0, 3)];

	switch ((int) BITS (20, 23)) {
	case 0:
		if (type == ARMul_FIRST) {
			/* First cycle of a busy wait.  */
			finish = ARMul_Time (state) + howlong;

			return howlong == 0 ? ARMul_DONE : ARMul_BUSY;
		}
		else if (type == ARMul_BUSY) {
			if (ARMul_Time (state) >= finish)
				return ARMul_DONE;
			else
				return ARMul_BUSY;
		}
		return ARMul_DONE;

	case 1:
		if (howlong == 0)
			ARMul_Abort (state, ARMul_FIQV);
		else
			ARMul_ScheduleEvent (state, howlong, DoAFIQ);
		return ARMul_DONE;

	case 2:
		if (howlong == 0)
			ARMul_Abort (state, ARMul_IRQV);
		else
			ARMul_ScheduleEvent (state, howlong, DoAIRQ);
		return ARMul_DONE;

	case 3:
		state->NfiqSig = HIGH;
		return ARMul_DONE;

	case 4:
		state->NirqSig = HIGH;
		return ARMul_DONE;

	case 5:
		ValReg[BITS (0, 3)] = ARMul_Time (state);
		return ARMul_DONE;
	}

	return ARMul_CANT;
}

/* Install co-processor instruction handlers in this routine.  */

unsigned
ARMul_CoProInit (ARMul_State * state)
{
	unsigned int i;

	/* Initialise tham all first.  */
	for (i = 0; i < 16; i++)
		ARMul_CoProDetach (state, i);

	/* Install CoPro Instruction handlers here.
	   The format is:
	   ARMul_CoProAttach (state, CP Number, Init routine, Exit routine
	   LDC routine, STC routine, MRC routine, MCR routine,
	   CDP routine, Read Reg routine, Write Reg routine).  */
	if (state->is_ep9312) {
		ARMul_CoProAttach (state, 4, NULL, NULL, DSPLDC4, DSPSTC4,
				   DSPMRC4, DSPMCR4, NULL, NULL, DSPCDP4, NULL, NULL);
		ARMul_CoProAttach (state, 5, NULL, NULL, DSPLDC5, DSPSTC5,
				   DSPMRC5, DSPMCR5, NULL, NULL, DSPCDP5, NULL, NULL);
		ARMul_CoProAttach (state, 6, NULL, NULL, NULL, NULL,
				   DSPMRC6, DSPMCR6, NULL, NULL, DSPCDP6, NULL, NULL);
	}
	else {
		ARMul_CoProAttach (state, 4, NULL, NULL, ValLDC, ValSTC,
				   ValMRC, ValMCR, NULL, NULL, ValCDP, NULL, NULL);

		ARMul_CoProAttach (state, 5, NULL, NULL, NULL, NULL,
				   ValMRC, ValMCR, NULL, NULL, IntCDP, NULL, NULL);
	}

	if (state->is_XScale) {
		//chy 2005-09-19, for PXA27x's CP6
		if (state->is_pxa27x) {
			ARMul_CoProAttach (state, 6, NULL, NULL,
					   NULL, NULL, xscale_cp6_mrc,
					   NULL, NULL, NULL, NULL, NULL, NULL);
		}
		//chy 2005-09-19 end------------- 
		ARMul_CoProAttach (state, 13, xscale_cp13_init,
				   xscale_cp13_exit, xscale_cp13_ldc,
				   xscale_cp13_stc, xscale_cp13_mrc,
				   xscale_cp13_mcr, NULL, NULL, xscale_cp13_cdp,
				   xscale_cp13_read_reg,
				   xscale_cp13_write_reg);

		ARMul_CoProAttach (state, 14, xscale_cp14_init,
				   xscale_cp14_exit, xscale_cp14_ldc,
				   xscale_cp14_stc, xscale_cp14_mrc,
				   xscale_cp14_mcr, NULL, NULL, xscale_cp14_cdp,
				   xscale_cp14_read_reg,
				   xscale_cp14_write_reg);
		//chy: 2003-08-24.
		ARMul_CoProAttach (state, 15, xscale_cp15_init,
				   xscale_cp15_exit, xscale_cp15_ldc,
				   xscale_cp15_stc, xscale_cp15_mrc,
				   xscale_cp15_mcr, NULL, NULL, xscale_cp15_cdp,
				   xscale_cp15_read_reg,
				   xscale_cp15_write_reg);
	}
	else if (state->is_v6) {
		ARMul_CoProAttach (state, 10, VFPInit, NULL, VFPLDC, VFPSTC,
				   VFPMRC, VFPMCR, VFPMRRC, VFPMCRR, VFPCDP, NULL, NULL);
		ARMul_CoProAttach (state, 11, VFPInit, NULL, VFPLDC, VFPSTC,
				   VFPMRC, VFPMCR, VFPMRRC, VFPMCRR, VFPCDP, NULL, NULL);
		
		ARMul_CoProAttach (state, 15, MMUInit, NULL, NULL, NULL,
				   MMUMRC, MMUMCR, NULL, NULL, NULL, NULL, NULL);
	}
	else {			//all except xscale
		ARMul_CoProAttach (state, 15, MMUInit, NULL, NULL, NULL,
				   //                  MMUMRC, MMUMCR, NULL, MMURead, MMUWrite);
				   MMUMRC, MMUMCR, NULL, NULL, NULL, NULL, NULL);
	}
//chy 2003-09-03 do it in future!!!!????
#if 0
	if (state->is_iWMMXt) {
		ARMul_CoProAttach (state, 0, NULL, NULL, IwmmxtLDC, IwmmxtSTC,
				   NULL, NULL, IwmmxtCDP, NULL, NULL);

		ARMul_CoProAttach (state, 1, NULL, NULL, NULL, NULL,
				   IwmmxtMRC, IwmmxtMCR, IwmmxtCDP, NULL,
				   NULL);
	}
#endif
	//-----------------------------------------------------------------------------
	//chy 2004-05-25, found the user/system code visit CP 1,2, so I add below code.
	ARMul_CoProAttach (state, 1, NULL, NULL, NULL, NULL,
			   ValMRC, ValMCR, NULL, NULL, NULL, NULL, NULL);
	ARMul_CoProAttach (state, 2, NULL, NULL, ValLDC, ValSTC,
			   NULL, NULL, NULL, NULL, NULL, NULL, NULL);
	//------------------------------------------------------------------------------
	/* No handlers below here.  */

	/* Call all the initialisation routines.  */
	for (i = 0; i < 16; i++)
		if (state->CPInit[i])
			(state->CPInit[i]) (state);

	return TRUE;
}

/* Install co-processor finalisation routines in this routine.  */

void
ARMul_CoProExit (ARMul_State * state)
{
	register unsigned i;

	for (i = 0; i < 16; i++)
		if (state->CPExit[i])
			(state->CPExit[i]) (state);

	for (i = 0; i < 16; i++)	/* Detach all handlers.  */
		ARMul_CoProDetach (state, i);
}

/* Routines to hook Co-processors into ARMulator.  */

void
ARMul_CoProAttach (ARMul_State * state,
		   unsigned number,
		   ARMul_CPInits * init,
		   ARMul_CPExits * exit,
		   ARMul_LDCs * ldc,
		   ARMul_STCs * stc,
		   ARMul_MRCs * mrc,
		   ARMul_MCRs * mcr,
		   ARMul_MRRCs * mrrc,
		   ARMul_MCRRs * mcrr,
		   ARMul_CDPs * cdp,
		   ARMul_CPReads * read, ARMul_CPWrites * write)
{
	if (init != NULL)
		state->CPInit[number] = init;
	if (exit != NULL)
		state->CPExit[number] = exit;
	if (ldc != NULL)
		state->LDC[number] = ldc;
	if (stc != NULL)
		state->STC[number] = stc;
	if (mrc != NULL)
		state->MRC[number] = mrc;
	if (mcr != NULL)
		state->MCR[number] = mcr;
	if (mrrc != NULL)
		state->MRRC[number] = mrrc;
	if (mcrr != NULL)
		state->MCRR[number] = mcrr;
	if (cdp != NULL)
		state->CDP[number] = cdp;
	if (read != NULL)
		state->CPRead[number] = read;
	if (write != NULL)
		state->CPWrite[number] = write;
}

void
ARMul_CoProDetach (ARMul_State * state, unsigned number)
{
	ARMul_CoProAttach (state, number, NULL, NULL,
			   NoCoPro4R, NoCoPro4W, NoCoPro4W, NoCoPro4R,
			   NoCoPro5W, NoCoPro5R, NoCoPro3R, NULL, NULL);

	state->CPInit[number] = NULL;
	state->CPExit[number] = NULL;
	state->CPRead[number] = NULL;
	state->CPWrite[number] = NULL;
}

//chy 2003-09-03:below funs just replace the old ones

/* Set the XScale FSR and FAR registers.  */

void
XScale_set_fsr_far (ARMul_State * state, ARMword fsr, ARMword _far)
{
	//if (!state->is_XScale || (read_cp14_reg (10) & (1UL << 31)) == 0)
	if (!state->is_XScale)
		return;
	//assume opcode2=0 crm =0
	xscale_cp15_write_reg (state, 5, fsr);
	xscale_cp15_write_reg (state, 6, _far);
}

//chy 2003-09-03 seems 0 is CANT, 1 is DONE ????
int
XScale_debug_moe (ARMul_State * state, int moe)
{
	//chy 2003-09-03 , W/R CP14 reg, now it's no use ????
	printf ("SKYEYE: XScale_debug_moe called !!!!\n");
	return 1;
}
