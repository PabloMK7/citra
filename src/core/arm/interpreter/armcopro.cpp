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

#include "core/arm/skyeye_common/armdefs.h"
#include "core/arm/skyeye_common/armemu.h"
#include "core/arm/skyeye_common/vfp/vfp.h"

// Dummy Co-processors.

static unsigned int NoCoPro3R(ARMul_State* state, unsigned int a, ARMword b)
{
    return ARMul_CANT;
}

static unsigned int NoCoPro4R(ARMul_State* state, unsigned int a, ARMword b, ARMword c)
{
    return ARMul_CANT;
}

static unsigned int NoCoPro4W(ARMul_State* state, unsigned int a, ARMword b, ARMword* c)
{
    return ARMul_CANT;
}

static unsigned int NoCoPro5R(ARMul_State* state, unsigned int a, ARMword b, ARMword c, ARMword d)
{
    return ARMul_CANT;
}

static unsigned int NoCoPro5W(ARMul_State* state, unsigned int a, ARMword b, ARMword* c, ARMword* d)
{
    return ARMul_CANT;
}

// Install co-processor instruction handlers in this routine.
unsigned int ARMul_CoProInit(ARMul_State* state)
{
    // Initialise tham all first.
    for (unsigned int i = 0; i < 16; i++)
        ARMul_CoProDetach(state, i);

    // Install CoPro Instruction handlers here.
    // The format is:
    // ARMul_CoProAttach (state, CP Number, Init routine, Exit routine
    // LDC routine, STC routine, MRC routine, MCR routine,
    // CDP routine, Read Reg routine, Write Reg routine).
    if (state->is_v6) {
        ARMul_CoProAttach(state, 10, VFPInit, NULL, VFPLDC, VFPSTC,
            VFPMRC, VFPMCR, VFPMRRC, VFPMCRR, VFPCDP, NULL, NULL);
        ARMul_CoProAttach(state, 11, VFPInit, NULL, VFPLDC, VFPSTC,
            VFPMRC, VFPMCR, VFPMRRC, VFPMCRR, VFPCDP, NULL, NULL);

        /*ARMul_CoProAttach (state, 15, MMUInit, NULL, NULL, NULL,
        MMUMRC, MMUMCR, NULL, NULL, NULL, NULL, NULL);*/
    }

    // No handlers below here.

    // Call all the initialisation routines.
    for (unsigned int i = 0; i < 16; i++)
        if (state->CPInit[i])
            (state->CPInit[i]) (state);

    return TRUE;
}

// Install co-processor finalisation routines in this routine.
void ARMul_CoProExit(ARMul_State * state)
{
    for (unsigned int i = 0; i < 16; i++)
        if (state->CPExit[i])
            (state->CPExit[i]) (state);

    // Detach all handlers.
    for (unsigned int i = 0; i < 16; i++)
        ARMul_CoProDetach(state, i);
}

// Routines to hook Co-processors into ARMulator.

void
ARMul_CoProAttach(ARMul_State* state,
unsigned number,
ARMul_CPInits* init,
ARMul_CPExits* exit,
ARMul_LDCs* ldc,
ARMul_STCs* stc,
ARMul_MRCs* mrc,
ARMul_MCRs* mcr,
ARMul_MRRCs* mrrc,
ARMul_MCRRs* mcrr,
ARMul_CDPs* cdp,
ARMul_CPReads* read, ARMul_CPWrites* write)
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

void ARMul_CoProDetach(ARMul_State* state, unsigned number)
{
    ARMul_CoProAttach(state, number, NULL, NULL,
        NoCoPro4R, NoCoPro4W, NoCoPro4W, NoCoPro4R,
        NoCoPro5W, NoCoPro5R, NoCoPro3R, NULL, NULL);

    state->CPInit[number] = NULL;
    state->CPExit[number] = NULL;
    state->CPRead[number] = NULL;
    state->CPWrite[number] = NULL;
}
