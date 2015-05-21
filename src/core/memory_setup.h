// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/memory.h"

namespace Memory {

const u32 PAGE_MASK = PAGE_SIZE - 1;
const int PAGE_BITS = 12;

void InitMemoryMap();

/**
 * Maps an allocated buffer onto a region of the emulated process address space.
 *
 * @param base The address to start mapping at. Must be page-aligned.
 * @param size The amount of bytes to map. Must be page-aligned.
 * @param target Buffer with the memory backing the mapping. Must be of length at least `size`.
 */
void MapMemoryRegion(VAddr base, u32 size, u8* target);

/**
 * Maps a region of the emulated process address space as a IO region.
 * @note Currently this can only be used to mark a region as being IO, since actual memory-mapped
 *       IO isn't yet supported.
 */
void MapIoRegion(VAddr base, u32 size);

void UnmapRegion(VAddr base, u32 size);

}
