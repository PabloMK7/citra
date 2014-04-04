/**
 * Copyright (C) 2014 Citra Emulator
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

enum {
    MEM_BOOTROM_SIZE        = 0x00010000,	///< Bootrom (super secret code/data @ 0x8000) size
    MEM_MPCORE_PRIV_SIZE    = 0x00002000,	///< MPCore private memory region size
    MEM_VRAM_SIZE           = 0x00600000,	///< VRAM size
    MEM_DSP_SIZE            = 0x00080000,	///< DSP memory size
    MEM_AXI_WRAM_SIZE       = 0x00080000,	///< AXI WRAM size
    MEM_FCRAM_SIZE          = 0x08000000,	///< FCRAM size... Really 0x07E00000, but power of 2
                                            //      works much better
    MEM_SCRATCHPAD_SIZE     = 0x00004000,  ///< Typical stack size - TODO: Read from exheader
                            
    MEM_VRAM_MASK           = 0x007FFFFF,
    MEM_FCRAM_MASK          = (MEM_FCRAM_SIZE - 1),	            ///< FCRAM mask
    MEM_SCRATCHPAD_MASK     = (MEM_SCRATCHPAD_SIZE - 1),           ///< Scratchpad memory mask
                            
    MEM_FCRAM_PADDR     = 0x20000000,                           ///< FCRAM physical address
    MEM_FCRAM_PADDR_END = (MEM_FCRAM_PADDR + MEM_FCRAM_SIZE),   ///< FCRAM end of physical space
    MEM_FCRAM_VADDR     = 0x08000000,                           ///< FCRAM virtual address
    MEM_FCRAM_VADDR_END = (MEM_FCRAM_VADDR + MEM_FCRAM_SIZE),   ///< FCRAM end of virtual space

    MEM_VRAM_VADDR          = 0x1F000000,
    MEM_SCRATCHPAD_VADDR    = (0x10000000 - MEM_SCRATCHPAD_SIZE),  ///< Scratchpad virtual address
};

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
extern u8* g_fcram;			///< Main memory
extern u8* g_vram;			///< Video memory (VRAM)
extern u8* g_scratchpad;    ///< Stack memory

void Init();
void Shutdown();

u8 Read8(const u32 addr);
u16 Read16(const u32 addr);
u32 Read32(const u32 addr);

u32 Read8_ZX(const u32 addr);
u32 Read16_ZX(const u32 addr);

void Write8(const u32 addr, const u8 data);
void Write16(const u32 addr, const u16 data);
void Write32(const u32 addr, const u32 data);

u8* GetPointer(const u32 Address);

} // namespace

////////////////////////////////////////////////////////////////////////////////////////////////////

#endif // CORE_MEM_MAP_H_