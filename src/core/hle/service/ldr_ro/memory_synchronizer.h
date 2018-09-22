// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "core/memory.h"

namespace Kernel {
class Process;
}

namespace Service::LDR {

/**
 * This is a work-around before we implement memory aliasing.
 * CRS and CRO are mapped (aliased) to another memory when loading. Games can read
 * from both the original buffer and the mapping memory. So we use this to synchronize
 * all original buffers with mapping memory after modifying the content.
 */
class MemorySynchronizer {
public:
    void Clear();

    void AddMemoryBlock(VAddr mapping, VAddr original, u32 size);
    void ResizeMemoryBlock(VAddr mapping, VAddr original, u32 size);
    void RemoveMemoryBlock(VAddr mapping, VAddr original);

    void SynchronizeOriginalMemory(Kernel::Process& process);

private:
    struct MemoryBlock {
        VAddr mapping;
        VAddr original;
        u32 size;
    };

    std::vector<MemoryBlock> memory_blocks;

    auto FindMemoryBlock(VAddr mapping, VAddr original);
};

} // namespace Service::LDR
