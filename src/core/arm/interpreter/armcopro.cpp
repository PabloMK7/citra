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

//chy 2005-07-08
//#include "ansidecl.h"
//chy -------
//#include "iwmmxt.h"

/* Dummy Co-processors.  */

static unsigned
NoCoPro3R(ARMul_State * state,
unsigned a, ARMword b)
{
    return ARMul_CANT;
}

static unsigned
NoCoPro4R(ARMul_State * state,
unsigned a,
ARMword b, ARMword c)
{
    return ARMul_CANT;
}

static unsigned
NoCoPro4W(ARMul_State * state,
unsigned a,
ARMword b, ARMword * c)
{
    return ARMul_CANT;
}

static unsigned
NoCoPro5R(ARMul_State * state,
unsigned a,
ARMword b,
ARMword c, ARMword d)
{
    return ARMul_CANT;
}

static unsigned
NoCoPro5W(ARMul_State * state,
unsigned a,
ARMword b,
ARMword * c, ARMword * d)
{
    return ARMul_CANT;
}

/* The XScale Co-processors.  */

/* Coprocessor 15:  System Control.  */
static void write_cp14_reg(unsigned, ARMword);
static ARMword read_cp14_reg(unsigned);

/* Check an access to a register.  */

static unsigned
check_cp15_access(ARMul_State * state,
unsigned reg,
unsigned CRm, unsigned opcode_1, unsigned opcode_2)
{
    /* Do not allow access to these register in USER mode.  */
    //chy 2006-02-16 , should not consider system mode, don't conside 26bit mode
    if (state->Mode == USER26MODE || state->Mode == USER32MODE)
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

/* Install co-processor instruction handlers in this routine.  */

unsigned
ARMul_CoProInit(ARMul_State * state)
{
    unsigned int i;

    /* Initialise tham all first.  */
    for (i = 0; i < 16; i++)
        ARMul_CoProDetach(state, i);

    /* Install CoPro Instruction handlers here.
    The format is:
    ARMul_CoProAttach (state, CP Number, Init routine, Exit routine
    LDC routine, STC routine, MRC routine, MCR routine,
    CDP routine, Read Reg routine, Write Reg routine).  */
    if (state->is_v6) {
        ARMul_CoProAttach(state, 10, VFPInit, NULL, VFPLDC, VFPSTC,
            VFPMRC, VFPMCR, VFPMRRC, VFPMCRR, VFPCDP, NULL, NULL);
        ARMul_CoProAttach(state, 11, VFPInit, NULL, VFPLDC, VFPSTC,
            VFPMRC, VFPMCR, VFPMRRC, VFPMCRR, VFPCDP, NULL, NULL);

        /*ARMul_CoProAttach (state, 15, MMUInit, NULL, NULL, NULL,
        MMUMRC, MMUMCR, NULL, NULL, NULL, NULL, NULL);*/
    }
    //chy 2003-09-03 do it in future!!!!????
#if 0
    if (state->is_iWMMXt) {
        ARMul_CoProAttach(state, 0, NULL, NULL, IwmmxtLDC, IwmmxtSTC,
            NULL, NULL, IwmmxtCDP, NULL, NULL);

        ARMul_CoProAttach(state, 1, NULL, NULL, NULL, NULL,
            IwmmxtMRC, IwmmxtMCR, IwmmxtCDP, NULL,
            NULL);
    }
#endif
    /* No handlers below here.  */

    /* Call all the initialisation routines.  */
    for (i = 0; i < 16; i++)
        if (state->CPInit[i])
            (state->CPInit[i]) (state);

    return TRUE;
}

/* Install co-processor finalisation routines in this routine.  */

void
ARMul_CoProExit(ARMul_State * state)
{
    register unsigned i;

    for (i = 0; i < 16; i++)
        if (state->CPExit[i])
            (state->CPExit[i]) (state);

    for (i = 0; i < 16; i++)	/* Detach all handlers.  */
        ARMul_CoProDetach(state, i);
}

/* Routines to hook Co-processors into ARMulator.  */

void
ARMul_CoProAttach(ARMul_State * state,
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
ARMul_CoProDetach(ARMul_State * state, unsigned number)
{
    ARMul_CoProAttach(state, number, NULL, NULL,
        NoCoPro4R, NoCoPro4W, NoCoPro4W, NoCoPro4R,
        NoCoPro5W, NoCoPro5R, NoCoPro3R, NULL, NULL);

    state->CPInit[number] = NULL;
    state->CPExit[number] = NULL;
    state->CPRead[number] = NULL;
    state->CPWrite[number] = NULL;
}
