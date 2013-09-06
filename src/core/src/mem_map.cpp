/**
 * Copyright (C) 2013 Akiru Emulator
 *
 * @file    mem_map.cpp
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

#include "common.h"
#include "mem_arena.h"

#include "mem_map.h"
#include "core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Memory {

	
u8*	g_mem_base			= NULL;		///< The base pointer to the auto-mirrored arena.

MemArena g_mem_arena;				///< The MemArena class

u8* g_fcram				= NULL;		///< Main memory (FCRAM) pointer
u8* g_vram				= NULL;		///< Video memory (VRAM) pointer

u8* g_physical_fcram	= NULL;		///< Main physical memory (FCRAM)
u8* g_physical_vram		= NULL;		///< Video physical memory (VRAM)

// We don't declare the IO region in here since its handled by other means.
static MemoryView g_mem_views[] =
{
	{NULL,		NULL,				0x00000000, MEM_BOOTROM_SIZE,		0},
	{NULL,		NULL,				0x00010000, MEM_BOOTROM_SIZE,		MV_MIRROR_PREVIOUS},
	{NULL,      NULL,				0x17E00000, MEM_MPCORE_PRIV_SIZE,	0},
	{&g_vram,   &g_physical_vram,	0x18000000, MEM_VRAM_SIZE,			0},
	{NULL,      NULL,				0x1FF00000, MEM_DSP_SIZE,			0},
	{NULL,      NULL,				0x1FF80000, MEM_AXI_WRAM_SIZE,		0},
	{&g_ram,    &g_physical_fcram,	0x20000000, MEM_FCRAM_SIZE,			MV_IS_PRIMARY_RAM},
};

static const int kNumMemViews = sizeof(g_mem_views) / sizeof(MemoryView);	///< Number of mem views

u8 Read8(const u32 addr) {
	return 0xDE;
}

u16 Read16(const u32 addr) {
	return 0xDEAD;
}

u32 Read32(const u32 addr) {
	return 0xDEADBEEF;
}

void Write8(const u32 addr, const u32 data) {
}

void Write16(const u32 addr, const u32 data) {
}

void Write32(const u32 addr, const u32 data) {
}

void Init() {
	int flags = 0;

	for (size_t i = 0; i < ARRAY_SIZE(g_mem_views); i++) {
		if (g_mem_views[i].flags & MV_IS_PRIMARY_RAM)
			g_mem_views[i].size = MEMORY_SIZE;
	}
	g_base = MemoryMap_Setup(g_mem_views, kNumMemViews, flags, &g_mem_arena);

	INFO_LOG(MEMMAP, "Memory system initialized. RAM at %p (mirror at 0 @ %p)", g_fcram, 
		g_physical_fcram);
}

void Shutdown() {
	u32 flags = 0;
	MemoryMap_Shutdown(g_mem_views, kNumMemViews, flags, &g_mem_arena);
	g_mem_arena.ReleaseSpace();
	g_mem_base = NULL;
	INFO_LOG(MEMMAP, "Memory system shut down.");
}



} // namespace
