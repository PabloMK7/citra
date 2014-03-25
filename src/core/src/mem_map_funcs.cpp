/**
 * Copyright (C) 2014 Citra Emulator
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

template <typename T>
inline void ReadFromHardware(T &var, const u32 addr)
{
	// TODO: Figure out the fastest order of tests for both read and write (they are probably different).
	// TODO: Make sure this represents the mirrors in a correct way.

	// Could just do a base-relative read, too.... TODO

	if ((addr & 0x3E000000) == 0x08000000) {
		var = *((const T*)&g_fcram[addr & MEM_FCRAM_MASK]);
	}
	/*else if ((addr & 0x3F800000) == 0x04000000) {
		var = *((const T*)&m_pVRAM[addr & VRAM_MASK]);
	}*/
	else {
		_assert_msg_(MEMMAP, false, "unknown hardware read");
		// WARN_LOG(MEMMAP, "ReadFromHardware: Invalid addr %08x PC %08x LR %08x", addr, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
	}
}

template <typename T>
inline void WriteToHardware(u32 addr, const T data) {
	NOTICE_LOG(MEMMAP, "Test1 %08X", addr);
	// ExeFS:/.code is loaded here:
	if ((addr & 0xFFF00000) == 0x00100000) {
		// TODO(ShizZy): This is dumb... handle correctly. From 3DBrew:
		// http://3dbrew.org/wiki/Memory_layout#ARM11_User-land_memory_regions
		// The ExeFS:/.code is loaded here, executables must be loaded to the 0x00100000 region when
		// the exheader "special memory" flag is clear. The 0x03F00000-byte size restriction only 
		// applies when this flag is clear. Executables are usually loaded to 0x14000000 when the 
		// exheader "special memory" flag is set, however this address can be arbitrary.
		*(T*)&g_fcram[addr & MEM_FCRAM_MASK] = data;
		NOTICE_LOG(MEMMAP, "Test2");
	// Heap mapped by ControlMemory:
	} else if ((addr & 0x3E000000) == 0x08000000) {
		// TODO(ShizZy): Writes to this virtual address should be put in physical memory at FCRAM + GSP
		// heap size... the following is writing to FCRAM + 0, which is actually supposed to be the 
		// application's GSP heap
		*(T*)&g_fcram[addr & MEM_FCRAM_MASK] = data;
	} else if ((addr & 0xFF000000) == 0x14000000) {
		_assert_msg_(MEMMAP, false, "umimplemented write to GSP heap");
	} else if ((addr & 0xFFF00000) == 0x1EC00000) {
		_assert_msg_(MEMMAP, false, "umimplemented write to IO registers");
	} else if ((addr & 0xFF000000) == 0x1F000000) {
		_assert_msg_(MEMMAP, false, "umimplemented write to VRAM");
	} else if ((addr & 0xFFF00000) == 0x1FF00000) {
		_assert_msg_(MEMMAP, false, "umimplemented write to DSP memory");
	} else if ((addr & 0xFFFF0000) == 0x1FF80000) {
		_assert_msg_(MEMMAP, false, "umimplemented write to Configuration Memory");
	} else if ((addr & 0xFFFFF000) == 0x1FF81000) {
		_assert_msg_(MEMMAP, false, "umimplemented write to shared page");
	} else {
		_assert_msg_(MEMMAP, false, "unknown hardware write");
	}
}

bool IsValidAddress(const u32 addr) {
	if ((addr & 0x3E000000) == 0x08000000) {
		return true;
	} else if ((addr & 0x3F800000) == 0x04000000) {
		return true;
	} else if ((addr & 0xBFFF0000) == 0x00010000) {
		return true;
	} else if ((addr & 0x3F000000) >= 0x08000000 && (addr & 0x3F000000) < 0x08000000 + MEM_FCRAM_MASK) {
		return true;
	} else {
		return false;
	}
}

u8 *GetPointer(const u32 addr) {
	// TODO(bunnei): Just a stub for now... ImplementMe!
	if ((addr & 0x3E000000) == 0x08000000) {
		return g_fcram + (addr & MEM_FCRAM_MASK);
	}
	//else if ((addr & 0x3F800000) == 0x04000000) {
	//	return g_vram + (addr & MEM_VRAM_MASK);
	//}
	//else if ((addr & 0x3F000000) >= 0x08000000 && (addr & 0x3F000000) < 0x08000000 + g_MemorySize) {
	//	return m_pRAM + (addr & g_MemoryMask);
	//}
	else {
		//ERROR_LOG(MEMMAP, "Unknown GetPointer %08x PC %08x LR %08x", addr, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
		ERROR_LOG(MEMMAP, "Unknown GetPointer %08x", addr);
		static bool reported = false;
		//if (!reported) {
		//	Reporting::ReportMessage("Unknown GetPointer %08x PC %08x LR %08x", addr, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
		//	reported = true;
		//}
		//if (!g_Config.bIgnoreBadMemAccess) {
		//	Core_EnableStepping(true);
		//	host->SetDebugMode(true);
		//}
		return 0;
	}
}

u8 Read8(const u32 addr) {
	u8 _var = 0;
	ReadFromHardware<u8>(_var, addr);
	return (u8)_var;
}

u16 Read16(const u32 addr) {
	u16_le _var = 0;
	ReadFromHardware<u16_le>(_var, addr);
	return (u16)_var;
}

u32 Read32(const u32 addr) {
	u32_le _var = 0;
	ReadFromHardware<u32_le>(_var, addr);
	return _var;
}

u64 Read64(const u32 addr) {
	u64_le _var = 0;
	ReadFromHardware<u64_le>(_var, addr);
	return _var;
}

u32 Read8_ZX(const u32 addr) {
	return (u32)Read8(addr);
}

u32 Read16_ZX(const u32 addr) {
	return (u32)Read16(addr);
}

void Write8(const u32 addr, const u8 data) {
	WriteToHardware<u8>(addr, data);
}

void Write16(const u32 addr, const u16 data) {
	WriteToHardware<u16_le>(addr, data);
}

void Write32(const u32 addr, const u32 data) {
	WriteToHardware<u32_le>(addr, data);
}

void Write64(const u32 addr, const u64 data) {
	WriteToHardware<u64_le>(addr, data);
}

} // namespace
