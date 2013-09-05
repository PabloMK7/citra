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

namespace Memory {

extern u8* g_ram;
extern u8* g_vram;

extern u32 g_memory_size;
extern u32 g_memory_mask;

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