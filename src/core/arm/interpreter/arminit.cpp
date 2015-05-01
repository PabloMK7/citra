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
#include "core/mem_map.h"
#include "core/arm/skyeye_common/armdefs.h"
#include "core/arm/skyeye_common/armemu.h"
#include "core/arm/skyeye_common/vfp/vfp.h"

/***************************************************************************\
*            Returns a new instantiation of the ARMulator's state           *
\***************************************************************************/
ARMul_State* ARMul_NewState(ARMul_State* state)
{
    state->Emulate = RUN;
    state->Mode = USER32MODE;

    state->lateabtSig = HIGH;
    state->bigendSig = LOW;

    return state;
}

/***************************************************************************\
*       Call this routine to set ARMulator to model a certain processor     *
\***************************************************************************/

void ARMul_SelectProcessor(ARMul_State* state, unsigned properties)
{
    state->is_v4  = (properties & (ARM_v4_Prop | ARM_v5_Prop)) != 0;
    state->is_v5  = (properties & ARM_v5_Prop) != 0;
    state->is_v5e = (properties & ARM_v5e_Prop) != 0;
    state->is_v6  = (properties & ARM_v6_Prop) != 0;
    state->is_v7  = (properties & ARM_v7_Prop) != 0;
}

// Resets certain MPCore CP15 values to their ARM-defined reset values.
static void ResetMPCoreCP15Registers(ARMul_State* cpu)
{
    // c0
    cpu->CP15[CP15_MAIN_ID]  = 0x410FB024;
    cpu->CP15[CP15_TLB_TYPE] = 0x00000800;
    cpu->CP15[CP15_PROCESSOR_FEATURE_0] = 0x00000111;
    cpu->CP15[CP15_PROCESSOR_FEATURE_1] = 0x00000001;
    cpu->CP15[CP15_DEBUG_FEATURE_0] = 0x00000002;
    cpu->CP15[CP15_MEMORY_MODEL_FEATURE_0] = 0x01100103;
    cpu->CP15[CP15_MEMORY_MODEL_FEATURE_1] = 0x10020302;
    cpu->CP15[CP15_MEMORY_MODEL_FEATURE_2] = 0x01222000;
    cpu->CP15[CP15_MEMORY_MODEL_FEATURE_3] = 0x00000000;
    cpu->CP15[CP15_ISA_FEATURE_0] = 0x00100011;
    cpu->CP15[CP15_ISA_FEATURE_1] = 0x12002111;
    cpu->CP15[CP15_ISA_FEATURE_2] = 0x11221011;
    cpu->CP15[CP15_ISA_FEATURE_3] = 0x01102131;
    cpu->CP15[CP15_ISA_FEATURE_4] = 0x00000141;

    // c1
    cpu->CP15[CP15_CONTROL] = 0x00054078;
    cpu->CP15[CP15_AUXILIARY_CONTROL] = 0x0000000F;
    cpu->CP15[CP15_COPROCESSOR_ACCESS_CONTROL] = 0x00000000;

    // c2
    cpu->CP15[CP15_TRANSLATION_BASE_TABLE_0] = 0x00000000;
    cpu->CP15[CP15_TRANSLATION_BASE_TABLE_1] = 0x00000000;
    cpu->CP15[CP15_TRANSLATION_BASE_CONTROL] = 0x00000000;

    // c3
    cpu->CP15[CP15_DOMAIN_ACCESS_CONTROL] = 0x00000000;

    // c7
    cpu->CP15[CP15_PHYS_ADDRESS] = 0x00000000;

    // c9
    cpu->CP15[CP15_DATA_CACHE_LOCKDOWN] = 0xFFFFFFF0;

    // c10
    cpu->CP15[CP15_TLB_LOCKDOWN] = 0x00000000;
    cpu->CP15[CP15_PRIMARY_REGION_REMAP] = 0x00098AA4;
    cpu->CP15[CP15_NORMAL_REGION_REMAP]  = 0x44E048E0;

    // c13
    cpu->CP15[CP15_PID] = 0x00000000;
    cpu->CP15[CP15_CONTEXT_ID]  = 0x00000000;
    cpu->CP15[CP15_THREAD_UPRW] = 0x00000000;
    cpu->CP15[CP15_THREAD_URO]  = 0x00000000;
    cpu->CP15[CP15_THREAD_PRW]  = 0x00000000;

    // c15
    cpu->CP15[CP15_PERFORMANCE_MONITOR_CONTROL]    = 0x00000000;
    cpu->CP15[CP15_MAIN_TLB_LOCKDOWN_VIRT_ADDRESS] = 0x00000000;
    cpu->CP15[CP15_MAIN_TLB_LOCKDOWN_PHYS_ADDRESS] = 0x00000000;
    cpu->CP15[CP15_MAIN_TLB_LOCKDOWN_ATTRIBUTE]    = 0x00000000;
    cpu->CP15[CP15_TLB_DEBUG_CONTROL] = 0x00000000;
}

/***************************************************************************\
* Call this routine to set up the initial machine state (or perform a RESET *
\***************************************************************************/
void ARMul_Reset(ARMul_State* state)
{
    VFPInit(state);

    state->Reg[15] = 0;
    state->Cpsr = INTBITS | SVC32MODE;
    state->Mode = SVC32MODE;
    state->Bank = SVCBANK;

    ResetMPCoreCP15Registers(state);

    state->NresetSig = HIGH;
    state->NfiqSig = HIGH;
    state->NirqSig = HIGH;
    state->NtransSig = (state->Mode & 3) ? HIGH : LOW;
    state->abortSig = LOW;

    state->NumInstrs = 0;
}
