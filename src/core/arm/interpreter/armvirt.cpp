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

#include "core/arm/skyeye_common/armdefs.h"
#include "core/arm/skyeye_common/armemu.h"

#include "core/mem_map.h"

#define dumpstack 1
#define dumpstacksize 0x10
#define maxdmupaddr 0x0033a850

/*ARMword ARMul_GetCPSR (ARMul_State * state) {
return 0;
}
ARMword ARMul_GetSPSR (ARMul_State * state, ARMword mode) {
return 0;
}
void ARMul_SetCPSR (ARMul_State * state, ARMword value) {

}
void ARMul_SetSPSR (ARMul_State * state, ARMword mode, ARMword value) {

}*/

void ARMul_Icycles(ARMul_State * state, unsigned number, ARMword address) {
}

void ARMul_Ccycles(ARMul_State * state, unsigned number, ARMword address) {
}

ARMword ARMul_LoadInstrS(ARMul_State * state, ARMword address, ARMword isize) {
    state->NumScycles++;

#ifdef HOURGLASS
    if ((state->NumScycles & HOURGLASS_RATE) == 0) {
        HOURGLASS;
    }
#endif
    if (isize == 2)
        return (u16)Memory::Read16(address);
    else
        return (u32)Memory::Read32(address);
}

ARMword ARMul_LoadInstrN(ARMul_State * state, ARMword address, ARMword isize) {
    state->NumNcycles++;

    if (isize == 2)
        return (u16)Memory::Read16(address);
    else
        return (u32)Memory::Read32(address);
}

ARMword ARMul_ReLoadInstr(ARMul_State * state, ARMword address, ARMword isize) {
    ARMword data;

    if ((isize == 2) && (address & 0x2)) {
        ARMword lo;
        lo = (u16)Memory::Read16(address);
        return lo;
    }

    data = (u32)Memory::Read32(address);
    return data;
}

ARMword ARMul_ReadWord(ARMul_State * state, ARMword address) {
    ARMword data;
    data = Memory::Read32(address);
    return data;
}

ARMword ARMul_LoadWordS(ARMul_State * state, ARMword address) {
    state->NumScycles++;
    return ARMul_ReadWord(state, address);
}

ARMword ARMul_LoadWordN(ARMul_State * state, ARMword address) {
    state->NumNcycles++;
    return ARMul_ReadWord(state, address);
}

ARMword ARMul_LoadHalfWord(ARMul_State * state, ARMword address) {
    state->NumNcycles++;
    return (u16)Memory::Read16(address);;
}

ARMword ARMul_ReadByte(ARMul_State * state, ARMword address) {
    return (u8)Memory::Read8(address);
}

ARMword ARMul_LoadByte(ARMul_State * state, ARMword address) {
    state->NumNcycles++;
    return ARMul_ReadByte(state, address);
}

void ARMul_StoreHalfWord(ARMul_State * state, ARMword address, ARMword data) {
    state->NumNcycles++;
    Memory::Write16(address, data);
}

void ARMul_StoreByte(ARMul_State * state, ARMword address, ARMword data) {
    state->NumNcycles++;
    ARMul_WriteByte(state, address, data);
}

ARMword ARMul_SwapWord(ARMul_State * state, ARMword address, ARMword data) {
    ARMword temp;
    state->NumNcycles++;
    temp = ARMul_ReadWord(state, address);
    state->NumNcycles++;
    Memory::Write32(address, data);
    return temp;
}

ARMword ARMul_SwapByte(ARMul_State * state, ARMword address, ARMword data) {
    ARMword temp;
    temp = ARMul_LoadByte(state, address);
    Memory::Write8(address, data);
    return temp;
}

void ARMul_WriteWord(ARMul_State * state, ARMword address, ARMword data) {
    Memory::Write32(address, data);
}

void ARMul_WriteByte(ARMul_State * state, ARMword address, ARMword data)
{
    Memory::Write8(address, data);
}

void ARMul_StoreWordS(ARMul_State * state, ARMword address, ARMword data)
{
    state->NumScycles++;
    ARMul_WriteWord(state, address, data);
}

void ARMul_StoreWordN(ARMul_State * state, ARMword address, ARMword data)
{
    state->NumNcycles++;
    ARMul_WriteWord(state, address, data);
}
