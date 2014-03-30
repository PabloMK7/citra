/*  armvirt.c -- ARMulator virtual memory interace:  ARM6 Instruction Emulator.
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

/* This file contains a complete ARMulator memory model, modelling a
"virtual memory" system. A much simpler model can be found in armfast.c,
and that model goes faster too, but has a fixed amount of memory. This
model's memory has 64K pages, allocated on demand from a 64K entry page
table. The routines PutWord and GetWord implement this. Pages are never
freed as they might be needed again. A single area of memory may be
defined to generate aborts. */

#include "armdefs.h"
#include "skyeye_defs.h"
//#include "code_cov.h"

#ifdef VALIDATE			/* for running the validate suite */
#define TUBE 48 * 1024 * 1024	/* write a char on the screen */
#define ABORTS 1
#endif

/* #define ABORTS */

#ifdef ABORTS			/* the memory system will abort */
/* For the old test suite Abort between 32 Kbytes and 32 Mbytes
   For the new test suite Abort between 8 Mbytes and 26 Mbytes */
/* #define LOWABORT 32 * 1024
#define HIGHABORT 32 * 1024 * 1024 */
#define LOWABORT 8 * 1024 * 1024
#define HIGHABORT 26 * 1024 * 1024

#endif

#define NUMPAGES 64 * 1024
#define PAGESIZE 64 * 1024
#define PAGEBITS 16
#define OFFSETBITS 0xffff
//chy 2003-08-19: seems no use ????
int SWI_vector_installed = FALSE;
extern ARMword skyeye_cachetype;

/***************************************************************************\
*        Get a byte into Virtual Memory, maybe allocating the page          *
\***************************************************************************/
static fault_t
GetByte (ARMul_State * state, ARMword address, ARMword * data)
{
	fault_t fault;

	fault = mmu_read_byte (state, address, data);
	if (fault) {
//chy 2003-07-11: sometime has fault, but linux can continue running  !!!!????
//              printf("SKYEYE: GetByte fault %d \n", fault);
	}
	return fault;
}

/***************************************************************************\
*        Get a halfword into Virtual Memory, maybe allocating the page          *
\***************************************************************************/
static fault_t
GetHalfWord (ARMul_State * state, ARMword address, ARMword * data)
{
	fault_t fault;

	fault = mmu_read_halfword (state, address, data);
	if (fault) {
//chy 2003-07-11: sometime has fault, but linux can continue running  !!!!????
//              printf("SKYEYE: GetHalfWord fault %d \n", fault);
	}
	return fault;
}

/***************************************************************************\
*        Get a Word from Virtual Memory, maybe allocating the page          *
\***************************************************************************/

static fault_t
GetWord (ARMul_State * state, ARMword address, ARMword * data)
{
	fault_t fault;

	fault = mmu_read_word (state, address, data);
	if (fault) {
//chy 2003-07-11: sometime has fault, but linux can continue running  !!!!????
#if 0
/* XXX */ extern int hack;
		hack = 1;
#endif
#if 0
		printf ("mmu_read_word at 0x%08x: ", address);
		switch (fault) {
		case ALIGNMENT_FAULT:
			printf ("ALIGNMENT_FAULT");
			break;
		case SECTION_TRANSLATION_FAULT:
			printf ("SECTION_TRANSLATION_FAULT");
			break;
		case PAGE_TRANSLATION_FAULT:
			printf ("PAGE_TRANSLATION_FAULT");
			break;
		case SECTION_DOMAIN_FAULT:
			printf ("SECTION_DOMAIN_FAULT");
			break;
		case SECTION_PERMISSION_FAULT:
			printf ("SECTION_PERMISSION_FAULT");
			break;
		case SUBPAGE_PERMISSION_FAULT:
			printf ("SUBPAGE_PERMISSION_FAULT");
			break;
		default:
			printf ("Unrecognized fault number!");
		}
		printf ("\tpc = 0x%08x\n", state->Reg[15]);
#endif
	}
	return fault;
}

//2003-07-10 chy: lyh change
/****************************************************************************\
 * 			Load a Instrion Word into Virtual Memory						*
\****************************************************************************/
static fault_t
LoadInstr (ARMul_State * state, ARMword address, ARMword * instr)
{
	fault_t fault;
	fault = mmu_load_instr (state, address, instr);
	return fault;
	//if (fault)
	//      log_msg("load_instr fault = %d, address = %x\n", fault, address);
}

/***************************************************************************\
*        Put a byte into Virtual Memory, maybe allocating the page          *
\***************************************************************************/
static fault_t
PutByte (ARMul_State * state, ARMword address, ARMword data)
{
	fault_t fault;

	fault = mmu_write_byte (state, address, data);
	if (fault) {
//chy 2003-07-11: sometime has fault, but linux can continue running  !!!!????
//              printf("SKYEYE: PutByte fault %d \n", fault);
	}
	return fault;
}

/***************************************************************************\
*        Put a halfword into Virtual Memory, maybe allocating the page          *
\***************************************************************************/
static fault_t
PutHalfWord (ARMul_State * state, ARMword address, ARMword data)
{
	fault_t fault;

	fault = mmu_write_halfword (state, address, data);
	if (fault) {
//chy 2003-07-11: sometime has fault, but linux can continue running  !!!!????
//              printf("SKYEYE: PutHalfWord fault %d \n", fault);
	}
	return fault;
}

/***************************************************************************\
*        Put a Word into Virtual Memory, maybe allocating the page          *
\***************************************************************************/

static fault_t
PutWord (ARMul_State * state, ARMword address, ARMword data)
{
	fault_t fault;

	fault = mmu_write_word (state, address, data);
	if (fault) {
//chy 2003-07-11: sometime has fault, but linux can continue running  !!!!????
#if 0
/* XXX */ extern int hack;
		hack = 1;
#endif
#if 0
		printf ("mmu_write_word at 0x%08x: ", address);
		switch (fault) {
		case ALIGNMENT_FAULT:
			printf ("ALIGNMENT_FAULT");
			break;
		case SECTION_TRANSLATION_FAULT:
			printf ("SECTION_TRANSLATION_FAULT");
			break;
		case PAGE_TRANSLATION_FAULT:
			printf ("PAGE_TRANSLATION_FAULT");
			break;
		case SECTION_DOMAIN_FAULT:
			printf ("SECTION_DOMAIN_FAULT");
			break;
		case SECTION_PERMISSION_FAULT:
			printf ("SECTION_PERMISSION_FAULT");
			break;
		case SUBPAGE_PERMISSION_FAULT:
			printf ("SUBPAGE_PERMISSION_FAULT");
			break;
		default:
			printf ("Unrecognized fault number!");
		}
		printf ("\tpc = 0x%08x\n", state->Reg[15]);
#endif
	}
	return fault;
}

/***************************************************************************\
*                      Initialise the memory interface                      *
\***************************************************************************/

unsigned
ARMul_MemoryInit (ARMul_State * state, unsigned int initmemsize)
{
	return TRUE;
}

/***************************************************************************\
*                         Remove the memory interface                       *
\***************************************************************************/

void
ARMul_MemoryExit (ARMul_State * state)
{
}

/***************************************************************************\
*                   ReLoad Instruction                                     *
\***************************************************************************/

ARMword
ARMul_ReLoadInstr (ARMul_State * state, ARMword address, ARMword isize)
{
	ARMword data;
	fault_t fault;

#ifdef ABORTS
	if (address >= LOWABORT && address < HIGHABORT) {
		ARMul_PREFETCHABORT (address);
		return ARMul_ABORTWORD;
	}
	else {
		ARMul_CLEARABORT;
	}
#endif
#if 0
	/* do profiling for code coverage */
	if (skyeye_config.code_cov.prof_on)
			cov_prof(EXEC_FLAG, address);
#endif
#if 1
	if ((isize == 2) && (address & 0x2)) {
		ARMword lo, hi;
		if (!(skyeye_cachetype == INSTCACHE))
			fault = GetHalfWord (state, address, &lo);
		else
			fault = LoadInstr (state, address, &lo);
#if 0
		if (!fault) {
			if (!(skyeye_cachetype == INSTCACHE))
				fault = GetHalfWord (state, address + isize, &hi);
			else
				fault = LoadInstr (state, address + isize, &hi);

		}
#endif
		if (fault) {
			ARMul_PREFETCHABORT (address);
			return ARMul_ABORTWORD;
		}
		else {
			ARMul_CLEARABORT;
		}
		return lo;
#if 0
		if (state->bigendSig == HIGH)
			return (lo << 16) | (hi >> 16);
		else
			return ((hi & 0xFFFF) << 16) | (lo >> 16);
#endif
	}
#endif
	if (!(skyeye_cachetype == INSTCACHE))
		fault = GetWord (state, address, &data);
	else
		fault = LoadInstr (state, address, &data);

	if (fault) {

		/* dyf add for s3c6410 no instcache temporary 2010.9.17 */
		if (!(skyeye_cachetype == INSTCACHE)) {
			/* set translation fault  on prefetch abort */
			state->mmu.fault_statusi = fault  & 0xFF;
			state->mmu.fault_address = address;
		}
		/* add end */

		ARMul_PREFETCHABORT (address);
		return ARMul_ABORTWORD;
	}
	else {
		ARMul_CLEARABORT;
	}

	return data;
}

/***************************************************************************\
*                   Load Instruction, Sequential Cycle                      *
\***************************************************************************/

ARMword
ARMul_LoadInstrS (ARMul_State * state, ARMword address, ARMword isize)
{
	state->NumScycles++;

#ifdef HOURGLASS
	if ((state->NumScycles & HOURGLASS_RATE) == 0) {
		HOURGLASS;
	}
#endif

	return ARMul_ReLoadInstr (state, address, isize);
}

/***************************************************************************\
*                 Load Instruction, Non Sequential Cycle                    *
\***************************************************************************/

ARMword
ARMul_LoadInstrN (ARMul_State * state, ARMword address, ARMword isize)
{
	state->NumNcycles++;

	return ARMul_ReLoadInstr (state, address, isize);
}

/***************************************************************************\
*                      Read Word (but don't tell anyone!)                   *
\***************************************************************************/

ARMword
ARMul_ReadWord (ARMul_State * state, ARMword address)
{
	ARMword data;
	fault_t fault;

#ifdef ABORTS
	if (address >= LOWABORT && address < HIGHABORT) {
		ARMul_DATAABORT (address);
		return ARMul_ABORTWORD;
	}
	else {
		ARMul_CLEARABORT;
	}
#endif

	fault = GetWord (state, address, &data);
	if (fault) {
		state->mmu.fault_status =
			(fault | (state->mmu.last_domain << 4)) & 0xFF;
		state->mmu.fault_address = address;
		ARMul_DATAABORT (address);
		return ARMul_ABORTWORD;
	}
	else {
		ARMul_CLEARABORT;
	}
	return data;
}

/***************************************************************************\
*                        Load Word, Sequential Cycle                        *
\***************************************************************************/

ARMword
ARMul_LoadWordS (ARMul_State * state, ARMword address)
{
	state->NumScycles++;

	return ARMul_ReadWord (state, address);
}

/***************************************************************************\
*                      Load Word, Non Sequential Cycle                      *
\***************************************************************************/

ARMword
ARMul_LoadWordN (ARMul_State * state, ARMword address)
{
	state->NumNcycles++;

	return ARMul_ReadWord (state, address);
}

/***************************************************************************\
*                     Load Halfword, (Non Sequential Cycle)                 *
\***************************************************************************/

ARMword
ARMul_LoadHalfWord (ARMul_State * state, ARMword address)
{
	ARMword data;
	fault_t fault;

	state->NumNcycles++;
	fault = GetHalfWord (state, address, &data);

	if (fault) {
		state->mmu.fault_status =
			(fault | (state->mmu.last_domain << 4)) & 0xFF;
		state->mmu.fault_address = address;
		ARMul_DATAABORT (address);
		return ARMul_ABORTWORD;
	}
	else {
		ARMul_CLEARABORT;
	}

	return data;

}

/***************************************************************************\
*                      Read Byte (but don't tell anyone!)                   *
\***************************************************************************/
int ARMul_ICE_ReadByte(ARMul_State * state, ARMword address, ARMword *presult)
{
 ARMword data;
 fault_t fault;
 fault = GetByte (state, address, &data);
 if (fault) {
	 *presult=-1; fault=ALIGNMENT_FAULT; return fault;
 }else{
	 *(char *)presult=(unsigned char)(data & 0xff); fault=NO_FAULT; return fault;
 }
}
	 
 
ARMword
ARMul_ReadByte (ARMul_State * state, ARMword address)
{
	ARMword data;
	fault_t fault;

	fault = GetByte (state, address, &data);

	if (fault) {
		state->mmu.fault_status =
			(fault | (state->mmu.last_domain << 4)) & 0xFF;
		state->mmu.fault_address = address;
		ARMul_DATAABORT (address);
		return ARMul_ABORTWORD;
	}
	else {
		ARMul_CLEARABORT;
	}

	return data;

}

/***************************************************************************\
*                     Load Byte, (Non Sequential Cycle)                     *
\***************************************************************************/

ARMword
ARMul_LoadByte (ARMul_State * state, ARMword address)
{
	state->NumNcycles++;

	return ARMul_ReadByte (state, address);
}

/***************************************************************************\
*                     Write Word (but don't tell anyone!)                   *
\***************************************************************************/

void
ARMul_WriteWord (ARMul_State * state, ARMword address, ARMword data)
{
	fault_t fault;

#ifdef ABORTS
	if (address >= LOWABORT && address < HIGHABORT) {
		ARMul_DATAABORT (address);
		return;
	}
	else {
		ARMul_CLEARABORT;
	}
#endif

	fault = PutWord (state, address, data);
	if (fault) {
		state->mmu.fault_status =
			(fault | (state->mmu.last_domain << 4)) & 0xFF;
		state->mmu.fault_address = address;
		ARMul_DATAABORT (address);
		return;
	}
	else {
		ARMul_CLEARABORT;
	}
}

/***************************************************************************\
*                       Store Word, Sequential Cycle                        *
\***************************************************************************/

void
ARMul_StoreWordS (ARMul_State * state, ARMword address, ARMword data)
{
	state->NumScycles++;

	ARMul_WriteWord (state, address, data);
}

/***************************************************************************\
*                       Store Word, Non Sequential Cycle                        *
\***************************************************************************/

void
ARMul_StoreWordN (ARMul_State * state, ARMword address, ARMword data)
{
	state->NumNcycles++;

	ARMul_WriteWord (state, address, data);
}

/***************************************************************************\
*                    Store HalfWord, (Non Sequential Cycle)                 *
\***************************************************************************/

void
ARMul_StoreHalfWord (ARMul_State * state, ARMword address, ARMword data)
{
	fault_t fault;
	state->NumNcycles++;
	fault = PutHalfWord (state, address, data);
	if (fault) {
		state->mmu.fault_status =
			(fault | (state->mmu.last_domain << 4)) & 0xFF;
		state->mmu.fault_address = address;
		ARMul_DATAABORT (address);
		return;
	}
	else {
		ARMul_CLEARABORT;
	}
}

//chy 2006-04-15 
int ARMul_ICE_WriteByte (ARMul_State * state, ARMword address, ARMword data)
{
	fault_t fault;
	fault = PutByte (state, address, data);
	if (fault) 
		return 1; 
	else 
		return 0;
}
/***************************************************************************\
*                     Write Byte (but don't tell anyone!)                   *
\***************************************************************************/
//chy 2003-07-10, add real write byte fun
void
ARMul_WriteByte (ARMul_State * state, ARMword address, ARMword data)
{
	fault_t fault;
	fault = PutByte (state, address, data);
	if (fault) {
		state->mmu.fault_status =
			(fault | (state->mmu.last_domain << 4)) & 0xFF;
		state->mmu.fault_address = address;
		ARMul_DATAABORT (address);
		return;
	}
	else {
		ARMul_CLEARABORT;
	}
}

/***************************************************************************\
*                    Store Byte, (Non Sequential Cycle)                     *
\***************************************************************************/

void
ARMul_StoreByte (ARMul_State * state, ARMword address, ARMword data)
{
	state->NumNcycles++;

#ifdef VALIDATE
	if (address == TUBE) {
		if (data == 4)
			state->Emulate = FALSE;
		else
			(void) putc ((char) data, stderr);	/* Write Char */
		return;
	}
#endif

	ARMul_WriteByte (state, address, data);
}

/***************************************************************************\
*                   Swap Word, (Two Non Sequential Cycles)                  *
\***************************************************************************/

ARMword
ARMul_SwapWord (ARMul_State * state, ARMword address, ARMword data)
{
	ARMword temp;

	state->NumNcycles++;

	temp = ARMul_ReadWord (state, address);

	state->NumNcycles++;

	PutWord (state, address, data);

	return temp;
}

/***************************************************************************\
*                   Swap Byte, (Two Non Sequential Cycles)                  *
\***************************************************************************/

ARMword
ARMul_SwapByte (ARMul_State * state, ARMword address, ARMword data)
{
	ARMword temp;

	temp = ARMul_LoadByte (state, address);
	ARMul_StoreByte (state, address, data);

	return temp;
}

/***************************************************************************\
*                             Count I Cycles                                *
\***************************************************************************/

void
ARMul_Icycles (ARMul_State * state, unsigned number,
	       ARMword address)
{
	state->NumIcycles += number;
	ARMul_CLEARABORT;
}

/***************************************************************************\
*                             Count C Cycles                                *
\***************************************************************************/

void
ARMul_Ccycles (ARMul_State * state, unsigned number,
	       ARMword address)
{
	state->NumCcycles += number;
	ARMul_CLEARABORT;
}
