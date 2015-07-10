// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Kernel {
class VMManager;
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
