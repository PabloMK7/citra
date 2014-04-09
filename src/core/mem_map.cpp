// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"
#include "common/mem_arena.h"

#include "core/mem_map.h"
#include "core/core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Memory {

    
u8*    g_base                   = NULL;         ///< The base pointer to the auto-mirrored arena.

MemArena g_arena;                               ///< The MemArena class

u8* g_bootrom                   = NULL;         ///< Bootrom physical memory
u8* g_fcram                     = NULL;         ///< Main memory (FCRAM) pointer
u8* g_vram                      = NULL;         ///< Video memory (VRAM) pointer
u8* g_scratchpad                = NULL;         ///< Scratchpad memory - Used for main thread stack

u8* g_physical_bootrom          = NULL;         ///< Bootrom physical memory
u8* g_uncached_bootrom          = NULL;

u8* g_physical_fcram            = NULL;         ///< Main physical memory (FCRAM)
u8* g_physical_vram             = NULL;         ///< Video physical memory (VRAM)
u8* g_physical_scratchpad       = NULL;         ///< Scratchpad memory used for main thread stack

// We don't declare the IO region in here since its handled by other means.
static MemoryView g_views[] = {
    { &g_vram,  &g_physical_vram,   MEM_VRAM_VADDR,     MEM_VRAM_SIZE,  0 },
    { &g_fcram, &g_physical_fcram,  MEM_FCRAM_VADDR,    MEM_FCRAM_SIZE, MV_IS_PRIMARY_RAM },
};

/*static MemoryView views[] =
{
    {&m_pScratchPad, &m_pPhysicalScratchPad,  0x00010000, SCRATCHPAD_SIZE, 0},
    {NULL,           &m_pUncachedScratchPad,  0x40010000, SCRATCHPAD_SIZE, MV_MIRROR_PREVIOUS},
    {&m_pVRAM,       &m_pPhysicalVRAM,        0x04000000, 0x00800000, 0},
    {NULL,           &m_pUncachedVRAM,        0x44000000, 0x00800000, MV_MIRROR_PREVIOUS},
    {&m_pRAM,        &m_pPhysicalRAM,         0x08000000, g_MemorySize, MV_IS_PRIMARY_RAM},    // only from 0x08800000 is it usable (last 24 megs)
    {NULL,           &m_pUncachedRAM,         0x48000000, g_MemorySize, MV_MIRROR_PREVIOUS | MV_IS_PRIMARY_RAM},
    {NULL,           &m_pKernelRAM,           0x88000000, g_MemorySize, MV_MIRROR_PREVIOUS | MV_IS_PRIMARY_RAM},

    // TODO: There are a few swizzled mirrors of VRAM, not sure about the best way to
    // implement those.
};*/

static const int kNumMemViews = sizeof(g_views) / sizeof(MemoryView);    ///< Number of mem views

void Init() {
    int flags = 0;

    for (size_t i = 0; i < ARRAY_SIZE(g_views); i++) {
        if (g_views[i].flags & MV_IS_PRIMARY_RAM)
            g_views[i].size = MEM_FCRAM_SIZE;
    }

    g_base = MemoryMap_Setup(g_views, kNumMemViews, flags, &g_arena);

    g_scratchpad = new u8[MEM_SCRATCHPAD_SIZE];

    NOTICE_LOG(MEMMAP, "Memory system initialized. RAM at %p (mirror at 0 @ %p)", g_fcram, 
        g_physical_fcram);
}

void Shutdown() {
    u32 flags = 0;
    MemoryMap_Shutdown(g_views, kNumMemViews, flags, &g_arena);
    
    g_arena.ReleaseSpace();
    delete[] g_scratchpad;
    
    g_base          = NULL;
    g_scratchpad    = NULL;

    NOTICE_LOG(MEMMAP, "Memory system shut down.");
}

} // namespace
