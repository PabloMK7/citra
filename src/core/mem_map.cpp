// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/mem_arena.h"

#include "core/mem_map.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Memory {

u8* g_base;                     ///< The base pointer to the auto-mirrored arena.

static MemArena arena;          ///< The MemArena class

u8* g_exefs_code;               ///< ExeFS:/.code is loaded here
u8* g_system_mem;               ///< System memory
u8* g_heap;                     ///< Application heap (main memory)
u8* g_heap_linear;              ///< Linear heap
u8* g_vram;                     ///< Video memory (VRAM) pointer
u8* g_shared_mem;               ///< Shared memory
u8* g_dsp_mem;                  ///< DSP memory
u8* g_kernel_mem;               ///< Kernel memory

static u8* physical_bootrom;    ///< Bootrom physical memory
static u8* uncached_bootrom;

static u8* physical_exefs_code; ///< Phsical ExeFS:/.code is loaded here
static u8* physical_system_mem; ///< System physical memory
static u8* physical_fcram;      ///< Main physical memory (FCRAM)
static u8* physical_heap_gsp;   ///< GSP heap physical memory
static u8* physical_vram;       ///< Video physical memory (VRAM)
static u8* physical_shared_mem; ///< Physical shared memory
static u8* physical_dsp_mem;    ///< Physical DSP memory
static u8* physical_kernel_mem; ///< Kernel memory

// We don't declare the IO region in here since its handled by other means.
static MemoryView g_views[] = {
    {&g_exefs_code,     &physical_exefs_code,   EXEFS_CODE_VADDR,       EXEFS_CODE_SIZE,    0},
    {&g_vram,           &physical_vram,         VRAM_VADDR,             VRAM_SIZE,          0},
    {&g_heap,           &physical_fcram,        HEAP_VADDR,             HEAP_SIZE,          MV_IS_PRIMARY_RAM},
    {&g_shared_mem,     &physical_shared_mem,   SHARED_MEMORY_VADDR,    SHARED_MEMORY_SIZE, 0},
    {&g_system_mem,     &physical_system_mem,   SYSTEM_MEMORY_VADDR,    SYSTEM_MEMORY_SIZE, 0},
    {&g_dsp_mem,        &physical_dsp_mem,      DSP_MEMORY_VADDR,       DSP_MEMORY_SIZE,    0},
    {&g_kernel_mem,     &physical_kernel_mem,   KERNEL_MEMORY_VADDR,    KERNEL_MEMORY_SIZE, 0},
    {&g_heap_linear,    &physical_heap_gsp,     HEAP_LINEAR_VADDR,      HEAP_LINEAR_SIZE,   0},
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

    g_base = MemoryMap_Setup(g_views, kNumMemViews, flags, &arena);
    MemBlock_Init();

    LOG_DEBUG(HW_Memory, "initialized OK, RAM at %p (mirror at 0 @ %p)", g_heap,
        physical_fcram);
}

void Shutdown() {
    u32 flags = 0;
    MemoryMap_Shutdown(g_views, kNumMemViews, flags, &arena);
    arena.ReleaseSpace();
    MemBlock_Shutdown();

    g_base = nullptr;
    g_exefs_code = nullptr;
    g_system_mem = nullptr;
    g_heap = nullptr;
    g_heap_linear = nullptr;
    g_vram = nullptr;
    g_shared_mem = nullptr;
    g_dsp_mem = nullptr;
    g_kernel_mem = nullptr;

    physical_bootrom = nullptr;
    uncached_bootrom = nullptr;
    physical_exefs_code = nullptr;
    physical_system_mem = nullptr;
    physical_fcram = nullptr;
    physical_heap_gsp = nullptr;
    physical_vram = nullptr;
    physical_shared_mem = nullptr;
    physical_dsp_mem = nullptr;
    physical_kernel_mem = nullptr;

    LOG_DEBUG(HW_Memory, "shutdown OK");
}

} // namespace
