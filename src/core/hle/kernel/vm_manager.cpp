// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iterator>
#include "common/assert.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/memory.h"
#include "core/memory_setup.h"
#include "core/mmio.h"

namespace Kernel {

static const char* GetMemoryStateName(MemoryState state) {
    static const char* names[] = {
        "Free",   "Reserved",   "IO",      "Static", "Code",      "Private",
        "Shared", "Continuous", "Aliased", "Alias",  "AliasCode", "Locked",
    };

    return names[(int)state];
}

bool VirtualMemoryArea::CanBeMergedWith(const VirtualMemoryArea& next) const {
    ASSERT(base + size == next.base);
    if (permissions != next.permissions || meminfo_state != next.meminfo_state ||
        type != next.type) {
        return false;
    }
    if (type == VMAType::AllocatedMemoryBlock &&
        (backing_block != next.backing_block || offset + size != next.offset)) {
        return false;
    }
    if (type == VMAType::BackingMemory && backing_memory + size != next.backing_memory) {
        return false;
    }
    if (type == VMAType::MMIO && paddr + size != next.paddr) {
        return false;
    }
    return true;
}

VMManager::VMManager() {
    Reset();
}

VMManager::~VMManager() {
    Reset();
}

void VMManager::Reset() {
    vma_map.clear();

    // Initialize the map with a single free region covering the entire managed space.
    VirtualMemoryArea initial_vma;
    initial_vma.size = MAX_ADDRESS;
    vma_map.emplace(initial_vma.base, initial_vma);

    UpdatePageTableForVMA(initial_vma);
}

VMManager::VMAHandle VMManager::FindVMA(VAddr target) const {
    if (target >= MAX_ADDRESS) {
        return vma_map.end();
    } else {
        return std::prev(vma_map.upper_bound(target));
    }
}

ResultVal<VMManager::VMAHandle> VMManager::MapMemoryBlock(VAddr target,
                                                          std::shared_ptr<std::vector<u8>> block,
                                                          size_t offset, u32 size,
                                                          MemoryState state) {
    ASSERT(block != nullptr);
    ASSERT(offset + size <= block->size());

    // This is the appropriately sized VMA that will turn into our allocation.
    CASCADE_RESULT(VMAIter vma_handle, CarveVMA(target, size));
    VirtualMemoryArea& final_vma = vma_handle->second;
    ASSERT(final_vma.size == size);

    final_vma.type = VMAType::AllocatedMemoryBlock;
    final_vma.permissions = VMAPermission::ReadWrite;
    final_vma.meminfo_state = state;
    final_vma.backing_block = block;
    final_vma.offset = offset;
    UpdatePageTableForVMA(final_vma);

    return MakeResult<VMAHandle>(MergeAdjacent(vma_handle));
}

ResultVal<VMManager::VMAHandle> VMManager::MapBackingMemory(VAddr target, u8* memory, u32 size,
                                                            MemoryState state) {
    ASSERT(memory != nullptr);

    // This is the appropriately sized VMA that will turn into our allocation.
    CASCADE_RESULT(VMAIter vma_handle, CarveVMA(target, size));
    VirtualMemoryArea& final_vma = vma_handle->second;
    ASSERT(final_vma.size == size);

    final_vma.type = VMAType::BackingMemory;
    final_vma.permissions = VMAPermission::ReadWrite;
    final_vma.meminfo_state = state;
    final_vma.backing_memory = memory;
    UpdatePageTableForVMA(final_vma);

    return MakeResult<VMAHandle>(MergeAdjacent(vma_handle));
}

ResultVal<VMManager::VMAHandle> VMManager::MapMMIO(VAddr target, PAddr paddr, u32 size,
                                                   MemoryState state,
                                                   Memory::MMIORegionPointer mmio_handler) {
    // This is the appropriately sized VMA that will turn into our allocation.
    CASCADE_RESULT(VMAIter vma_handle, CarveVMA(target, size));
    VirtualMemoryArea& final_vma = vma_handle->second;
    ASSERT(final_vma.size == size);

    final_vma.type = VMAType::MMIO;
    final_vma.permissions = VMAPermission::ReadWrite;
    final_vma.meminfo_state = state;
    final_vma.paddr = paddr;
    final_vma.mmio_handler = mmio_handler;
    UpdatePageTableForVMA(final_vma);

    return MakeResult<VMAHandle>(MergeAdjacent(vma_handle));
}

VMManager::VMAIter VMManager::Unmap(VMAIter vma_handle) {
    VirtualMemoryArea& vma = vma_handle->second;
    vma.type = VMAType::Free;
    vma.permissions = VMAPermission::None;
    vma.meminfo_state = MemoryState::Free;

    vma.backing_block = nullptr;
    vma.offset = 0;
    vma.backing_memory = nullptr;
    vma.paddr = 0;

    UpdatePageTableForVMA(vma);

    return MergeAdjacent(vma_handle);
}

ResultCode VMManager::UnmapRange(VAddr target, u32 size) {
    CASCADE_RESULT(VMAIter vma, CarveVMARange(target, size));
    VAddr target_end = target + size;

    VMAIter end = vma_map.end();
    // The comparison against the end of the range must be done using addresses since VMAs can be
    // merged during this process, causing invalidation of the iterators.
    while (vma != end && vma->second.base < target_end) {
        vma = std::next(Unmap(vma));
    }

    ASSERT(FindVMA(target)->second.size >= size);
    return RESULT_SUCCESS;
}

VMManager::VMAHandle VMManager::Reprotect(VMAHandle vma_handle, VMAPermission new_perms) {
    VMAIter iter = StripIterConstness(vma_handle);

    VirtualMemoryArea& vma = iter->second;
    vma.permissions = new_perms;
    UpdatePageTableForVMA(vma);

    return MergeAdjacent(iter);
}

ResultCode VMManager::ReprotectRange(VAddr target, u32 size, VMAPermission new_perms) {
    CASCADE_RESULT(VMAIter vma, CarveVMARange(target, size));
    VAddr target_end = target + size;

    VMAIter end = vma_map.end();
    // The comparison against the end of the range must be done using addresses since VMAs can be
    // merged during this process, causing invalidation of the iterators.
    while (vma != end && vma->second.base < target_end) {
        vma = std::next(StripIterConstness(Reprotect(vma, new_perms)));
    }

    return RESULT_SUCCESS;
}

void VMManager::RefreshMemoryBlockMappings(const std::vector<u8>* block) {
    // If this ever proves to have a noticeable performance impact, allow users of the function to
    // specify a specific range of addresses to limit the scan to.
    for (const auto& p : vma_map) {
        const VirtualMemoryArea& vma = p.second;
        if (block == vma.backing_block.get()) {
            UpdatePageTableForVMA(vma);
        }
    }
}

void VMManager::LogLayout(Log::Level log_level) const {
    for (const auto& p : vma_map) {
        const VirtualMemoryArea& vma = p.second;
        LOG_GENERIC(Log::Class::Kernel, log_level, "%08X - %08X  size: %8X %c%c%c %s", vma.base,
                    vma.base + vma.size, vma.size,
                    (u8)vma.permissions & (u8)VMAPermission::Read ? 'R' : '-',
                    (u8)vma.permissions & (u8)VMAPermission::Write ? 'W' : '-',
                    (u8)vma.permissions & (u8)VMAPermission::Execute ? 'X' : '-',
                    GetMemoryStateName(vma.meminfo_state));
    }
}

VMManager::VMAIter VMManager::StripIterConstness(const VMAHandle& iter) {
    // This uses a neat C++ trick to convert a const_iterator to a regular iterator, given
    // non-const access to its container.
    return vma_map.erase(iter, iter); // Erases an empty range of elements
}

ResultVal<VMManager::VMAIter> VMManager::CarveVMA(VAddr base, u32 size) {
    ASSERT_MSG((size & Memory::PAGE_MASK) == 0, "non-page aligned size: 0x%8X", size);
    ASSERT_MSG((base & Memory::PAGE_MASK) == 0, "non-page aligned base: 0x%08X", base);

    VMAIter vma_handle = StripIterConstness(FindVMA(base));
    if (vma_handle == vma_map.end()) {
        // Target address is outside the range managed by the kernel
        return ERR_INVALID_ADDRESS;
    }

    VirtualMemoryArea& vma = vma_handle->second;
    if (vma.type != VMAType::Free) {
        // Region is already allocated
        return ERR_INVALID_ADDRESS_STATE;
    }

    u32 start_in_vma = base - vma.base;
    u32 end_in_vma = start_in_vma + size;

    if (end_in_vma > vma.size) {
        // Requested allocation doesn't fit inside VMA
        return ERR_INVALID_ADDRESS_STATE;
    }

    if (end_in_vma != vma.size) {
        // Split VMA at the end of the allocated region
        SplitVMA(vma_handle, end_in_vma);
    }
    if (start_in_vma != 0) {
        // Split VMA at the start of the allocated region
        vma_handle = SplitVMA(vma_handle, start_in_vma);
    }

    return MakeResult<VMAIter>(vma_handle);
}

ResultVal<VMManager::VMAIter> VMManager::CarveVMARange(VAddr target, u32 size) {
    ASSERT_MSG((size & Memory::PAGE_MASK) == 0, "non-page aligned size: 0x%8X", size);
    ASSERT_MSG((target & Memory::PAGE_MASK) == 0, "non-page aligned base: 0x%08X", target);

    VAddr target_end = target + size;
    ASSERT(target_end >= target);
    ASSERT(target_end <= MAX_ADDRESS);
    ASSERT(size > 0);

    VMAIter begin_vma = StripIterConstness(FindVMA(target));
    VMAIter i_end = vma_map.lower_bound(target_end);
    for (auto i = begin_vma; i != i_end; ++i) {
        if (i->second.type == VMAType::Free) {
            return ERR_INVALID_ADDRESS_STATE;
        }
    }

    if (target != begin_vma->second.base) {
        begin_vma = SplitVMA(begin_vma, target - begin_vma->second.base);
    }

    VMAIter end_vma = StripIterConstness(FindVMA(target_end));
    if (end_vma != vma_map.end() && target_end != end_vma->second.base) {
        end_vma = SplitVMA(end_vma, target_end - end_vma->second.base);
    }

    return MakeResult<VMAIter>(begin_vma);
}

VMManager::VMAIter VMManager::SplitVMA(VMAIter vma_handle, u32 offset_in_vma) {
    VirtualMemoryArea& old_vma = vma_handle->second;
    VirtualMemoryArea new_vma = old_vma; // Make a copy of the VMA

    // For now, don't allow no-op VMA splits (trying to split at a boundary) because it's probably
    // a bug. This restriction might be removed later.
    ASSERT(offset_in_vma < old_vma.size);
    ASSERT(offset_in_vma > 0);

    old_vma.size = offset_in_vma;
    new_vma.base += offset_in_vma;
    new_vma.size -= offset_in_vma;

    switch (new_vma.type) {
    case VMAType::Free:
        break;
    case VMAType::AllocatedMemoryBlock:
        new_vma.offset += offset_in_vma;
        break;
    case VMAType::BackingMemory:
        new_vma.backing_memory += offset_in_vma;
        break;
    case VMAType::MMIO:
        new_vma.paddr += offset_in_vma;
        break;
    }

    ASSERT(old_vma.CanBeMergedWith(new_vma));

    return vma_map.emplace_hint(std::next(vma_handle), new_vma.base, new_vma);
}

VMManager::VMAIter VMManager::MergeAdjacent(VMAIter iter) {
    VMAIter next_vma = std::next(iter);
    if (next_vma != vma_map.end() && iter->second.CanBeMergedWith(next_vma->second)) {
        iter->second.size += next_vma->second.size;
        vma_map.erase(next_vma);
    }

    if (iter != vma_map.begin()) {
        VMAIter prev_vma = std::prev(iter);
        if (prev_vma->second.CanBeMergedWith(iter->second)) {
            prev_vma->second.size += iter->second.size;
            vma_map.erase(iter);
            iter = prev_vma;
        }
    }

    return iter;
}

void VMManager::UpdatePageTableForVMA(const VirtualMemoryArea& vma) {
    switch (vma.type) {
    case VMAType::Free:
        Memory::UnmapRegion(vma.base, vma.size);
        break;
    case VMAType::AllocatedMemoryBlock:
        Memory::MapMemoryRegion(vma.base, vma.size, vma.backing_block->data() + vma.offset);
        break;
    case VMAType::BackingMemory:
        Memory::MapMemoryRegion(vma.base, vma.size, vma.backing_memory);
        break;
    case VMAType::MMIO:
        Memory::MapIoRegion(vma.base, vma.size, vma.mmio_handler);
        break;
    }
}
}
