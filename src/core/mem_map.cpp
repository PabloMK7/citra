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

u8* g_exefs_code                = NULL;         ///< ExeFS:/.code is loaded here
u8* g_heap                      = NULL;         ///< Application heap (main memory)
u8* g_heap_gsp                  = NULL;         ///< GSP heap (main memory)
u8* g_vram                      = NULL;         ///< Video memory (VRAM) pointer
u8* g_shared_mem                = NULL;         ///< Shared memory

u8* g_physical_bootrom          = NULL;         ///< Bootrom physical memory
u8* g_uncached_bootrom          = NULL;

u8* g_physical_exefs_code       = NULL;         ///< Phsical ExeFS:/.code is loaded here
u8* g_physical_fcram            = NULL;         ///< Main physical memory (FCRAM)
u8* g_physical_heap_gsp         = NULL;         ///< GSP heap physical memory
u8* g_physical_vram             = NULL;         ///< Video physical memory (VRAM)
u8* g_physical_shared_mem       = NULL;         ///< Physical shared memory

// We don't declare the IO region in here since its handled by other means.
static MemoryView g_views[] = {
    {&g_exefs_code, &g_physical_exefs_code, EXEFS_CODE_VADDR,       EXEFS_CODE_SIZE,    0},
    {&g_vram,       &g_physical_vram,       VRAM_VADDR,             VRAM_SIZE,          0},
    {&g_heap,       &g_physical_fcram,      HEAP_VADDR,             HEAP_SIZE,          MV_IS_PRIMARY_RAM},
    {&g_shared_mem, &g_physical_shared_mem, SHARED_MEMORY_VADDR,    SHARED_MEMORY_SIZE, 0},
    {&g_heap_gsp,   &g_physical_heap_gsp,   HEAP_GSP_VADDR,         HEAP_GSP_SIZE,      0},
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
            g_views[i].size = FCRAM_SIZE;
    }

    g_base = MemoryMap_Setup(g_views, kNumMemViews, flags, &g_arena);

    NOTICE_LOG(MEMMAP, "initialized OK, RAM at %p (mirror at 0 @ %p)", g_heap, 
        g_physical_fcram);
}

void Shutdown() {
    u32 flags = 0;
    MemoryMap_Shutdown(g_views, kNumMemViews, flags, &g_arena);
    
    g_arena.ReleaseSpace();
    g_base = NULL;

    NOTICE_LOG(MEMMAP, "shutdown OK");
}

} // namespace
