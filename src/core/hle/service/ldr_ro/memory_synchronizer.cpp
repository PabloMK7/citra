// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/assert.h"
#include "core/hle/service/ldr_ro/memory_synchronizer.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace LDR_RO

namespace LDR_RO {

auto MemorySynchronizer::FindMemoryBlock(VAddr mapping, VAddr original) {
    auto block = std::find_if(memory_blocks.begin(), memory_blocks.end(),
                              [=](MemoryBlock& b) { return b.original == original; });
    ASSERT(block->mapping == mapping);
    return block;
}

void MemorySynchronizer::Clear() {
    memory_blocks.clear();
}

void MemorySynchronizer::AddMemoryBlock(VAddr mapping, VAddr original, u32 size) {
    memory_blocks.push_back(MemoryBlock{mapping, original, size});
}

void MemorySynchronizer::ResizeMemoryBlock(VAddr mapping, VAddr original, u32 size) {
    FindMemoryBlock(mapping, original)->size = size;
}

void MemorySynchronizer::RemoveMemoryBlock(VAddr mapping, VAddr original) {
    memory_blocks.erase(FindMemoryBlock(mapping, original));
}

void MemorySynchronizer::SynchronizeOriginalMemory() {
    for (auto& block : memory_blocks) {
        Memory::CopyBlock(block.original, block.mapping, block.size);
    }
}

} // namespace
