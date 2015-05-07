/*
    armmmu.c - Memory Management Unit emulation.
    ARMulator extensions for the ARM7100 family.
    Copyright (C) 1999  Ben Williamson

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
    Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
*/

#pragma once

#include "common/swap.h"

#include "core/mem_map.h"
#include "core/arm/skyeye_common/armdefs.h"

// Register numbers in the MMU
enum
{
    MMU_ID = 0,
    MMU_CONTROL = 1,
    MMU_TRANSLATION_TABLE_BASE = 2,
    MMU_DOMAIN_ACCESS_CONTROL = 3,
    MMU_FAULT_STATUS = 5,
    MMU_FAULT_ADDRESS = 6,
    MMU_CACHE_OPS = 7,
    MMU_TLB_OPS = 8,
    MMU_CACHE_LOCKDOWN = 9,
    MMU_TLB_LOCKDOWN = 10,
    MMU_PID = 13,

    // MMU_V4
    MMU_V4_CACHE_OPS = 7,
    MMU_V4_TLB_OPS = 8,

    // MMU_V3
    MMU_V3_FLUSH_TLB = 5,
    MMU_V3_FLUSH_TLB_ENTRY = 6,
    MMU_V3_FLUSH_CACHE = 7,
};

// Reads data in big/little endian format based on the
// state of the E (endian) bit in the emulated CPU's APSR.
inline u16 ReadMemory16(ARMul_State* cpu, u32 address) {
    u16 data = Memory::Read16(address);

    if (InBigEndianMode(cpu))
        data = Common::swap16(data);

    return data;
}

inline u32 ReadMemory32(ARMul_State* cpu, u32 address) {
    u32 data = Memory::Read32(address);

    if (InBigEndianMode(cpu))
        data = Common::swap32(data);

    return data;
}

inline u64 ReadMemory64(ARMul_State* cpu, u32 address) {
    u64 data = Memory::Read64(address);

    if (InBigEndianMode(cpu))
        data = Common::swap64(data);

    return data;
}

// Writes data in big/little endian format based on the
// state of the E (endian) bit in the emulated CPU's APSR.
inline void WriteMemory16(ARMul_State* cpu, u32 address, u16 data) {
    if (InBigEndianMode(cpu))
        data = Common::swap16(data);

    Memory::Write16(address, data);
}

inline void WriteMemory32(ARMul_State* cpu, u32 address, u32 data) {
    if (InBigEndianMode(cpu))
        data = Common::swap32(data);

    Memory::Write32(address, data);
}

inline void WriteMemory64(ARMul_State* cpu, u32 address, u64 data) {
    if (InBigEndianMode(cpu))
        data = Common::swap64(data);

    Memory::Write64(address, data);
}
