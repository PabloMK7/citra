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

#include <cstring>
#include "core/arm/skyeye_common/armdefs.h"
#include "core/arm/skyeye_common/armemu.h"

/***************************************************************************\
*            Returns a new instantiation of the ARMulator's state           *
\***************************************************************************/
ARMul_State* ARMul_NewState(ARMul_State* state)
{
    memset(state, 0, sizeof(ARMul_State));

    state->Emulate = RUN;
    for (unsigned int i = 0; i < 16; i++) {
        state->Reg[i] = 0;
        for (unsigned int j = 0; j < 7; j++)
            state->RegBank[j][i] = 0;
    }
    for (unsigned int i = 0; i < 7; i++)
        state->Spsr[i] = 0;

    state->Mode = USER32MODE;

    state->VectorCatch = 0;
    state->Aborted = false;
    state->Reseted = false;
    state->Inted = 3;
    state->LastInted = 3;

    state->lateabtSig = HIGH;
    state->bigendSig = LOW;

    return state;
}

/***************************************************************************\
*       Call this routine to set ARMulator to model a certain processor     *
\***************************************************************************/

void ARMul_SelectProcessor(ARMul_State* state, unsigned properties)
{
    state->is_v4     = (properties & (ARM_v4_Prop | ARM_v5_Prop)) != 0;
    state->is_v5     = (properties & ARM_v5_Prop) != 0;
    state->is_v5e    = (properties & ARM_v5e_Prop) != 0;
    state->is_v6     = (properties & ARM_v6_Prop) != 0;
    state->is_v7     = (properties & ARM_v7_Prop) != 0;

    // Only initialse the coprocessor support once we
    // know what kind of chip we are dealing with.
    ARMul_CoProInit(state);
}

/***************************************************************************\
* Call this routine to set up the initial machine state (or perform a RESET *
\***************************************************************************/
void ARMul_Reset(ARMul_State* state)
{
    state->NextInstr = 0;

    state->Reg[15] = 0;
    state->Cpsr = INTBITS | SVC32MODE;
    state->Mode = SVC32MODE;

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
}
