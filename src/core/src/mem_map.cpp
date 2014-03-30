/**
 * Copyright (C) 2014 Citra Emulator
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

	
u8*	g_base			= NULL;		///< The base pointer to the auto-mirrored arena.

MemArena g_arena;				///< The MemArena class

u8* g_bootrom			= NULL;		///< Bootrom memory (super secret code/data @ 0x8000) pointer
u8* g_fcram				= NULL;		///< Main memory (FCRAM) pointer
u8* g_vram				= NULL;		///< Video memory (VRAM) pointer

u8* g_physical_bootrom	= NULL;		///< Bootrom physical memory (super secret code/data @ 0x8000)
u8* g_uncached_bootrom	= NULL;

u8* g_physical_fcram	= NULL;		///< Main physical memory (FCRAM)
u8* g_physical_vram		= NULL;		///< Video physical memory (VRAM)

// We don't declare the IO region in here since its handled by other means.
static MemoryView g_views[] =
{
	{&g_bootrom,	&g_physical_bootrom,	0x00000000, MEM_BOOTROM_SIZE,		0},
	{NULL,			&g_uncached_bootrom,	0x00010000, MEM_BOOTROM_SIZE,		MV_MIRROR_PREVIOUS},
//	//{NULL,				NULL,					0x17E00000, MEM_MPCORE_PRIV_SIZE,	0},
	{&g_vram,		&g_physical_vram,		0x18000000, MEM_VRAM_SIZE,			0},
//	//{NULL,				NULL,					0x1FF00000, MEM_DSP_SIZE,			0},
//	//{NULL,				NULL,					0x1FF80000, MEM_AXI_WRAM_SIZE,		0},
	{&g_fcram,		&g_physical_fcram,		0x20000000, MEM_FCRAM_SIZE,			MV_IS_PRIMARY_RAM},
};

/*static MemoryView views[] =
{
	{&m_pScratchPad, &m_pPhysicalScratchPad,  0x00010000, SCRATCHPAD_SIZE, 0},
	{NULL,           &m_pUncachedScratchPad,  0x40010000, SCRATCHPAD_SIZE, MV_MIRROR_PREVIOUS},
	{&m_pVRAM,       &m_pPhysicalVRAM,        0x04000000, 0x00800000, 0},
	{NULL,           &m_pUncachedVRAM,        0x44000000, 0x00800000, MV_MIRROR_PREVIOUS},
	{&m_pRAM,        &m_pPhysicalRAM,         0x08000000, g_MemorySize, MV_IS_PRIMARY_RAM},	// only from 0x08800000 is it usable (last 24 megs)
	{NULL,           &m_pUncachedRAM,         0x48000000, g_MemorySize, MV_MIRROR_PREVIOUS | MV_IS_PRIMARY_RAM},
	{NULL,           &m_pKernelRAM,           0x88000000, g_MemorySize, MV_MIRROR_PREVIOUS | MV_IS_PRIMARY_RAM},

	// TODO: There are a few swizzled mirrors of VRAM, not sure about the best way to
	// implement those.
};*/

static const int kNumMemViews = sizeof(g_views) / sizeof(MemoryView);	///< Number of mem views

void Init() {
	int flags = 0;

	for (size_t i = 0; i < ARRAY_SIZE(g_views); i++) {
		if (g_views[i].flags & MV_IS_PRIMARY_RAM)
			g_views[i].size = MEM_FCRAM_SIZE;
	}

	g_base = MemoryMap_Setup(g_views, kNumMemViews, flags, &g_arena);

	NOTICE_LOG(MEMMAP, "Memory system initialized. RAM at %p (mirror at 0 @ %p)", g_fcram, 
		g_physical_fcram);
}

void Shutdown() {
	u32 flags = 0;
	MemoryMap_Shutdown(g_views, kNumMemViews, flags, &g_arena);
	g_arena.ReleaseSpace();
	g_base = NULL;
	NOTICE_LOG(MEMMAP, "Memory system shut down.");
}



} // namespace
