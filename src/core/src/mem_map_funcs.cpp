/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    mem_map_funcs.cpp
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2013-09-18
 * @brief   Memory map R/W functions
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include "common.h"

#include "mem_map.h"

namespace Memory {

/*
u8 *GetPointer(const u32 address)
{
	if ((address & 0x3E000000) == 0x08000000) {
		return g_fcram + (address & MEM_FCRAM_MASK);
	}
	else if ((address & 0x3F800000) == 0x04000000) {
		return m_pVRAM + (address & VRAM_MASK);
	}
	else if ((address & 0x3F000000) >= 0x08000000 && (address & 0x3F000000) < 0x08000000 + g_MemorySize) {
		return m_pRAM + (address & g_MemoryMask);
	}
	else {
		ERROR_LOG(MEMMAP, "Unknown GetPointer %08x PC %08x LR %08x", address, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
		static bool reported = false;
		if (!reported) {
			Reporting::ReportMessage("Unknown GetPointer %08x PC %08x LR %08x", address, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
			reported = true;
		}
		if (!g_Config.bIgnoreBadMemAccess) {
			Core_EnableStepping(true);
			host->SetDebugMode(true);
		}
		return 0;
	}
}*/

template <typename T>
inline void ReadFromHardware(T &var, const u32 address)
{
	// TODO: Figure out the fastest order of tests for both read and write (they are probably different).
	// TODO: Make sure this represents the mirrors in a correct way.

	// Could just do a base-relative read, too.... TODO

	if ((address & 0x3E000000) == 0x08000000) {
		var = *((const T*)&g_fcram[address & MEM_FCRAM_MASK]);
	}
	/*else if ((address & 0x3F800000) == 0x04000000) {
		var = *((const T*)&m_pVRAM[address & VRAM_MASK]);
	}*/
	else {
		_assert_msg_(MEMMAP, false, "unknown hardware read");
		// WARN_LOG(MEMMAP, "ReadFromHardware: Invalid address %08x PC %08x LR %08x", address, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
	}
}

template <typename T>
inline void WriteToHardware(u32 address, const T data)
{
	// Could just do a base-relative write, too.... TODO

	if ((address & 0x3E000000) == 0x08000000) {
		*(T*)&g_fcram[address & MEM_FCRAM_MASK] = data;
	}
	/*else if ((address & 0x3F800000) == 0x04000000) {
		*(T*)&m_pVRAM[address & VRAM_MASK] = data;
	}*/
	else {
		_assert_msg_(MEMMAP, false, "unknown hardware write");
		// WARN_LOG(MEMMAP, "WriteToHardware: Invalid address %08x	PC %08x LR %08x", address, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
	}
}

bool IsValidAddress(const u32 address) {
	if ((address & 0x3E000000) == 0x08000000) {
		return true;
	} else if ((address & 0x3F800000) == 0x04000000) {
		return true;
	} else if ((address & 0xBFFF0000) == 0x00010000) {
		return true;
	} else if ((address & 0x3F000000) >= 0x08000000 && (address & 0x3F000000) < 0x08000000 + MEM_FCRAM_MASK) {
		return true;
	} else {
		return false;
	}
}

u8 Read_U8(const u32 _Address) {
	u8 _var = 0;
	ReadFromHardware<u8>(_var, _Address);
	return (u8)_var;
}

u16 Read_U16(const u32 _Address) {
	u16_le _var = 0;
	ReadFromHardware<u16_le>(_var, _Address);
	return (u16)_var;
}

u32 Read_U32(const u32 _Address) {
	u32_le _var = 0;
	ReadFromHardware<u32_le>(_var, _Address);
	return _var;
}

u64 Read_U64(const u32 _Address) {
	u64_le _var = 0;
	ReadFromHardware<u64_le>(_var, _Address);
	return _var;
}

u32 Read_U8_ZX(const u32 _Address) {
	return (u32)Read_U8(_Address);
}

u32 Read_U16_ZX(const u32 _Address) {
	return (u32)Read_U16(_Address);
}

void Write_U8(const u8 _Data, const u32 _Address) {
	WriteToHardware<u8>(_Address, _Data);
}

void Write_U16(const u16 _Data, const u32 _Address) {
	WriteToHardware<u16_le>(_Address, _Data);
}

void Write_U32(const u32 _Data, const u32 _Address) {
	WriteToHardware<u32_le>(_Address, _Data);
}

void Write_U64(const u64 _Data, const u32 _Address) {
	WriteToHardware<u64_le>(_Address, _Data);
}

} // namespace
