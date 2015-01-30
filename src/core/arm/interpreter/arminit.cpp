/*  arminit.c -- ARMulator initialization:  ARM6 Instruction Emulator.
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

#include "core/arm/skyeye_common/armdefs.h"
#include "core/arm/skyeye_common/armemu.h"

/***************************************************************************\
*                 Definitions for the emulator architecture                 *
\***************************************************************************/

void ARMul_EmulateInit();
ARMul_State* ARMul_NewState(ARMul_State* state);
void ARMul_Reset (ARMul_State* state);
ARMword ARMul_DoCycle(ARMul_State* state);
unsigned ARMul_DoCoPro(ARMul_State* state);
ARMword ARMul_DoProg(ARMul_State* state);
ARMword ARMul_DoInstr(ARMul_State* state);
void ARMul_Abort(ARMul_State* state, ARMword address);

unsigned ARMul_MultTable[32] = {
    1, 2, 2, 3, 3, 4, 4, 5, 5, 6, 6, 7, 7, 8, 8, 9, 9,
    10, 10, 11, 11, 12, 12, 13, 13, 14, 14, 15, 15, 16, 16, 16
};
ARMword ARMul_ImmedTable[4096]; /* immediate DP LHS values */
char ARMul_BitList[256];        /* number of bits in a byte table */

void arm_dyncom_Abort(ARMul_State * state, ARMword vector)
{
    ARMul_Abort(state, vector);
}


/***************************************************************************\
*         Call this routine once to set up the emulator's tables.           *
\***************************************************************************/
void ARMul_EmulateInit()
{
    unsigned int i, j;

    for (i = 0; i < 4096; i++) {	/* the values of 12 bit dp rhs's */
        ARMul_ImmedTable[i] = ROTATER (i & 0xffL, (i >> 7L) & 0x1eL);
    }

    for (i = 0; i < 256; ARMul_BitList[i++] = 0);	/* how many bits in LSM */
    for (j = 1; j < 256; j <<= 1)
        for (i = 0; i < 256; i++)
            if ((i & j) > 0)
                ARMul_BitList[i]++;

    for (i = 0; i < 256; i++)
        ARMul_BitList[i] *= 4;	/* you always need 4 times these values */

}

/***************************************************************************\
*            Returns a new instantiation of the ARMulator's state           *
\***************************************************************************/
ARMul_State* ARMul_NewState(ARMul_State* state)
{
    unsigned i, j;

    memset (state, 0, sizeof (ARMul_State));

    state->Emulate = RUN;
    for (i = 0; i < 16; i++) {
        state->Reg[i] = 0;
        for (j = 0; j < 7; j++)
            state->RegBank[j][i] = 0;
    }
    for (i = 0; i < 7; i++)
        state->Spsr[i] = 0;
    state->Mode = 0;

    state->Debug = FALSE;
    state->VectorCatch = 0;
    state->Aborted = FALSE;
    state->Reseted = FALSE;
    state->Inted = 3;
    state->LastInted = 3;

#ifdef ARM61
    state->prog32Sig = LOW;
    state->data32Sig = LOW;
#else
    state->prog32Sig = HIGH;
    state->data32Sig = HIGH;
#endif

    state->lateabtSig = HIGH;
    state->bigendSig = LOW;

    //chy:2003-08-19
    state->CP14R0_CCD = -1;

    memset(&state->exclusive_tag_array[0], 0xFF, sizeof(state->exclusive_tag_array[0]) * 128);
    state->exclusive_access_state = 0;

    return state;
}

/***************************************************************************\
*       Call this routine to set ARMulator to model a certain processor     *
\***************************************************************************/

void ARMul_SelectProcessor(ARMul_State* state, unsigned properties)
{
    if (properties & ARM_Fix26_Prop) {
        state->prog32Sig = LOW;
        state->data32Sig = LOW;
    } else {
        state->prog32Sig = HIGH;
        state->data32Sig = HIGH;
    }

    state->is_v4     = (properties & (ARM_v4_Prop | ARM_v5_Prop)) ? HIGH : LOW;
    state->is_v5     = (properties & ARM_v5_Prop) ? HIGH : LOW;
    state->is_v5e    = (properties & ARM_v5e_Prop) ? HIGH : LOW;
    state->is_XScale = (properties & ARM_XScale_Prop) ? HIGH : LOW;
    state->is_iWMMXt = (properties & ARM_iWMMXt_Prop) ? HIGH : LOW;
    state->is_v6     = (properties & ARM_v6_Prop) ? HIGH : LOW;
    state->is_ep9312 = (properties & ARM_ep9312_Prop) ? HIGH : LOW;
    state->is_pxa27x = (properties & ARM_PXA27X_Prop) ? HIGH : LOW;
    state->is_v7     = (properties & ARM_v7_Prop) ? HIGH : LOW;

    /* Only initialse the coprocessor support once we
       know what kind of chip we are dealing with.  */
    //ARMul_CoProInit (state);

}

/***************************************************************************\
* Call this routine to set up the initial machine state (or perform a RESET *
\***************************************************************************/
void ARMul_Reset(ARMul_State* state)
{
    state->NextInstr = 0;
    if (state->prog32Sig) {
        state->Reg[15] = 0;
        state->Cpsr = INTBITS | SVC32MODE;
        state->Mode = SVC32MODE;
    } else {
        state->Reg[15] = R15INTBITS | SVC26MODE;
        state->Cpsr = INTBITS | SVC26MODE;
        state->Mode = SVC26MODE;
    }

    state->Bank = SVCBANK;
    FLUSHPIPE;

    state->EndCondition = 0;
    state->ErrorCode = 0;

    state->NresetSig = HIGH;
    state->NfiqSig = HIGH;
    state->NirqSig = HIGH;
    state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
    state->abortSig = LOW;
    state->AbortAddr = 1;

    state->NumInstrs = 0;
    state->NumNcycles = 0;
    state->NumScycles = 0;
    state->NumIcycles = 0;
    state->NumCcycles = 0;
    state->NumFcycles = 0;
}


/***************************************************************************\
* Emulate the execution of an entire program.  Start the correct emulator   *
* (Emulate26 for a 26 bit ARM and Emulate32 for a 32 bit ARM), return the   *
* address of the last instruction that is executed.                         *
\***************************************************************************/
ARMword ARMul_DoProg(ARMul_State* state)
{
    ARMword pc = 0;

    state->Emulate = RUN;
    while (state->Emulate != STOP) {
        state->Emulate = RUN;

        if (state->prog32Sig && ARMul_MODE32BIT) {
            pc = ARMul_Emulate32 (state);
        }
        else {
            //pc = ARMul_Emulate26 (state);
        }
    }

    return pc;
}

/***************************************************************************\
* Emulate the execution of one instruction.  Start the correct emulator     *
* (Emulate26 for a 26 bit ARM and Emulate32 for a 32 bit ARM), return the   *
* address of the instruction that is executed.                              *
\***************************************************************************/
ARMword ARMul_DoInstr(ARMul_State* state)
{
    ARMword pc = 0;

    state->Emulate = ONCE;

    if (state->prog32Sig && ARMul_MODE32BIT) {
        pc = ARMul_Emulate32 (state);
    }

    return pc;
}

/***************************************************************************\
* This routine causes an Abort to occur, including selecting the correct    *
* mode, register bank, and the saving of registers.  Call with the          *
* appropriate vector's memory address (0,4,8 ....)                          *
\***************************************************************************/
void ARMul_Abort(ARMul_State* state, ARMword vector)
{
    ARMword temp;
    int isize = INSN_SIZE;
    int esize = (TFLAG ? 0 : 4);
    int e2size = (TFLAG ? -4 : 0);

    state->Aborted = FALSE;

    if (state->prog32Sig)
        if (ARMul_MODE26BIT)
            temp = R15PC;
        else
            temp = state->Reg[15];
    else
        temp = R15PC | ECC | ER15INT | EMODE;

    switch (vector) {
    case ARMul_ResetV:	/* RESET */
        SETABORT (INTBITS, state->prog32Sig ? SVC32MODE : SVC26MODE,
                  0);
        break;
    case ARMul_UndefinedInstrV:	/* Undefined Instruction */
        SETABORT (IBIT, state->prog32Sig ? UNDEF32MODE : SVC26MODE,
                  isize);
        break;
    case ARMul_SWIV:	/* Software Interrupt */
        SETABORT (IBIT, state->prog32Sig ? SVC32MODE : SVC26MODE,
                  isize);
        break;
    case ARMul_PrefetchAbortV:	/* Prefetch Abort */
        state->AbortAddr = 1;
        SETABORT (IBIT, state->prog32Sig ? ABORT32MODE : SVC26MODE,
                  esize);
        break;
    case ARMul_DataAbortV:	/* Data Abort */
        SETABORT (IBIT, state->prog32Sig ? ABORT32MODE : SVC26MODE,
                  e2size);
        break;
    case ARMul_AddrExceptnV:	/* Address Exception */
        SETABORT (IBIT, SVC26MODE, isize);
        break;
    case ARMul_IRQV:	/* IRQ */
            SETABORT (IBIT,
                      state->prog32Sig ? IRQ32MODE : IRQ26MODE,
                      esize);
        break;
    case ARMul_FIQV:	/* FIQ */
            SETABORT (INTBITS,
                      state->prog32Sig ? FIQ32MODE : FIQ26MODE,
                      esize);
        break;
    }

    if (ARMul_MODE32BIT) {
        /*if (state->mmu.control & CONTROL_VECTOR)
          vector += 0xffff0000;	//for v4 high exception  address*/
        if (state->vector_remap_flag)
            vector += state->vector_remap_addr; /* support some remap function in LPC processor */
        ARMul_SetR15 (state, vector);
    } else
        ARMul_SetR15 (state, R15CCINTMODE | vector);
}
