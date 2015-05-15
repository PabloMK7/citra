// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Memory {

extern u8* g_exefs_code;  ///< ExeFS:/.code is loaded here
extern u8* g_heap;        ///< Application heap (main memory)
extern u8* g_shared_mem;  ///< Shared memory
extern u8* g_heap_linear; ///< Linear heap (main memory)
extern u8* g_vram;        ///< Video memory (VRAM)
extern u8* g_dsp_mem;     ///< DSP memory
extern u8* g_tls_mem;     ///< TLS memory

void Init();
void Shutdown();

/**
 * Maps a block of memory on the heap
 * @param size Size of block in bytes
 * @param operation Memory map operation type
 * @param permissions Memory allocation permissions
 */
u32 MapBlock_Heap(u32 size, u32 operation, u32 permissions);

/**
 * Maps a block of memory on the GSP heap
 * @param size Size of block in bytes
 * @param operation Memory map operation type
 * @param permissions Control memory permissions
 */
u32 MapBlock_HeapLinear(u32 size, u32 operation, u32 permissions);

/**
 * Converts a virtual address inside a region with 1:1 mapping to physical memory to a physical
 * address. This should be used by services to translate addresses for use by the hardware.
 */
PAddr VirtualToPhysicalAddress(VAddr addr);

/**
 * Undoes a mapping performed by VirtualToPhysicalAddress().
 */
VAddr PhysicalToVirtualAddress(PAddr addr);

} // namespace
