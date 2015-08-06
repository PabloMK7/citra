// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "common/common_types.h"

#include "core/hle/kernel/process.h"

namespace Kernel {

class VMManager;

struct MemoryRegionInfo {
    u32 base; // Not an address, but offset from start of FCRAM
    u32 size;

    std::shared_ptr<std::vector<u8>> linear_heap_memory;
};

void MemoryInit(u32 mem_type);
void MemoryShutdown();
MemoryRegionInfo* GetMemoryRegion(MemoryRegion region);

}

namespace Memory {

void Init();
void InitLegacyAddressSpace(Kernel::VMManager& address_space);
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

} // namespace
