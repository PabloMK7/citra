/**
 * Copyright (C) 2013 Akiru Emulator
 *
 * @file    mem_map.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2013-09-05
 * @brief   Memory map - handles virtual to physical memory access
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

#ifndef CORE_MEM_MAP_H_
#define CORE_MEM_MAP_H_

////////////////////////////////////////////////////////////////////////////////////////////////////

#include "common.h"
#include "common_types.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

#define MEM_BOOTROM_SIZE		0x00010000	///< Bootrom (super secret code/data @ 0x8000) size
#define MEM_MPCORE_PRIV_SIZE	0x00002000	///< MPCore private memory region size
#define MEM_VRAM_SIZE			0x00600000	///< VRAM size
#define MEM_DSP_SIZE			0x00080000	///< DSP memory size
#define MEM_AXI_WRAM_SIZE		0x00080000	///< AXI WRAM size
#define MEM_FCRAM_SIZE			0x08000000	///< FCRAM size

#define MEMORY_SIZE				MEM_FCRAM_SIZE
#define MEMORY_MASK				(MEM_FCRAM_SIZE - 1)	///< Main memory mask

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Memory {

// Base is a pointer to the base of the memory map. Yes, some MMU tricks
// are used to set up a full GC or Wii memory map in process memory.  on
// 32-bit, you have to mask your offsets with 0x3FFFFFFF. This means that
// some things are mirrored too many times, but eh... it works.

// In 64-bit, this might point to "high memory" (above the 32-bit limit),
// so be sure to load it into a 64-bit register.
extern u8 *g_base; 

// These are guaranteed to point to "low memory" addresses (sub-32-bit).
// 64-bit: Pointers to low-mem (sub-0x10000000) mirror
// 32-bit: Same as the corresponding physical/virtual pointers.
extern u8* g_ram;			///< Main memory
extern u8* g_vram;			///< Video memory (VRAM)

void Init();
void Shutdown();

u8 Read8(const u32 addr);
u16 Read16(const u32 addr);
u32 Read32(const u32 addr);

void Write8(const u32 addr, const u32 data);
void Write16(const u32 addr, const u32 data);
void Write32(const u32 addr, const u32 data);

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // CORE_MEM_MAP_H_