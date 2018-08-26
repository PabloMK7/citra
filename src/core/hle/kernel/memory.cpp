// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <map>
#include <memory>
#include <utility>
#include <vector>
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/core.h"
#include "core/hle/config_mem.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/hle/result.h"
#include "core/memory.h"
#include "core/memory_setup.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Kernel {

MemoryRegionInfo memory_regions[3];

/// Size of the APPLICATION, SYSTEM and BASE memory regions (respectively) for each system
/// memory configuration type.
static const u32 memory_region_sizes[8][3] = {
    // Old 3DS layouts
    {0x04000000, 0x02C00000, 0x01400000}, // 0
    {/* This appears to be unused. */},   // 1
    {0x06000000, 0x00C00000, 0x01400000}, // 2
    {0x05000000, 0x01C00000, 0x01400000}, // 3
    {0x04800000, 0x02400000, 0x01400000}, // 4
    {0x02000000, 0x04C00000, 0x01400000}, // 5

    // New 3DS layouts
    {0x07C00000, 0x06400000, 0x02000000}, // 6
    {0x0B200000, 0x02E00000, 0x02000000}, // 7
};

void MemoryInit(u32 mem_type) {
    // TODO(yuriks): On the n3DS, all o3DS configurations (<=5) are forced to 6 instead.
    ASSERT_MSG(mem_type <= 5, "New 3DS memory configuration aren't supported yet!");
    ASSERT(mem_type != 1);

    // The kernel allocation regions (APPLICATION, SYSTEM and BASE) are laid out in sequence, with
    // the sizes specified in the memory_region_sizes table.
    VAddr base = 0;
    for (int i = 0; i < 3; ++i) {
        memory_regions[i].base = base;
        memory_regions[i].size = memory_region_sizes[mem_type][i];
        memory_regions[i].used = 0;
        memory_regions[i].linear_heap_memory = std::make_shared<std::vector<u8>>();
        // Reserve enough space for this region of FCRAM.
        // We do not want this block of memory to be relocated when allocating from it.
        memory_regions[i].linear_heap_memory->reserve(memory_regions[i].size);

        base += memory_regions[i].size;
    }

    // We must've allocated the entire FCRAM by the end
    ASSERT(base == Memory::FCRAM_SIZE);

    using ConfigMem::config_mem;
    config_mem.app_mem_type = mem_type;
    // app_mem_malloc does not always match the configured size for memory_region[0]: in case the
    // n3DS type override is in effect it reports the size the game expects, not the real one.
    config_mem.app_mem_alloc = memory_region_sizes[mem_type][0];
    config_mem.sys_mem_alloc = memory_regions[1].size;
    config_mem.base_mem_alloc = memory_regions[2].size;
}

void MemoryShutdown() {
    for (auto& region : memory_regions) {
        region.base = 0;
        region.size = 0;
        region.used = 0;
        region.linear_heap_memory = nullptr;
    }
}

MemoryRegionInfo* GetMemoryRegion(MemoryRegion region) {
    switch (region) {
    case MemoryRegion::APPLICATION:
        return &memory_regions[0];
    case MemoryRegion::SYSTEM:
        return &memory_regions[1];
    case MemoryRegion::BASE:
        return &memory_regions[2];
    default:
        UNREACHABLE();
    }
}

void HandleSpecialMapping(VMManager& address_space, const AddressMapping& mapping) {
    using namespace Memory;

    struct MemoryArea {
        VAddr vaddr_base;
        PAddr paddr_base;
        u32 size;
    };

    // The order of entries in this array is important. The VRAM and IO VAddr ranges overlap, and
    // VRAM must be tried first.
    static constexpr MemoryArea memory_areas[] = {
        {VRAM_VADDR, VRAM_PADDR, VRAM_SIZE},
        {IO_AREA_VADDR, IO_AREA_PADDR, IO_AREA_SIZE},
        {DSP_RAM_VADDR, DSP_RAM_PADDR, DSP_RAM_SIZE},
        {N3DS_EXTRA_RAM_VADDR, N3DS_EXTRA_RAM_PADDR, N3DS_EXTRA_RAM_SIZE - 0x20000},
    };

    VAddr mapping_limit = mapping.address + mapping.size;
    if (mapping_limit < mapping.address) {
        LOG_CRITICAL(Loader, "Mapping size overflowed: address=0x{:08X} size=0x{:X}",
                     mapping.address, mapping.size);
        return;
    }

    auto area =
        std::find_if(std::begin(memory_areas), std::end(memory_areas), [&](const auto& area) {
            return mapping.address >= area.vaddr_base &&
                   mapping_limit <= area.vaddr_base + area.size;
        });
    if (area == std::end(memory_areas)) {
        LOG_ERROR(Loader,
                  "Unhandled special mapping: address=0x{:08X} size=0x{:X}"
                  " read_only={} unk_flag={}",
                  mapping.address, mapping.size, mapping.read_only, mapping.unk_flag);
        return;
    }

    u32 offset_into_region = mapping.address - area->vaddr_base;
    if (area->paddr_base == IO_AREA_PADDR) {
        LOG_ERROR(Loader, "MMIO mappings are not supported yet. phys_addr=0x{:08X}",
                  area->paddr_base + offset_into_region);
        return;
    }

    u8* target_pointer = Memory::GetPhysicalPointer(area->paddr_base + offset_into_region);

    // TODO(yuriks): This flag seems to have some other effect, but it's unknown what
    MemoryState memory_state = mapping.unk_flag ? MemoryState::Static : MemoryState::IO;

    auto vma =
        address_space.MapBackingMemory(mapping.address, target_pointer, mapping.size, memory_state)
            .Unwrap();
    address_space.Reprotect(vma,
                            mapping.read_only ? VMAPermission::Read : VMAPermission::ReadWrite);
}

void MapSharedPages(VMManager& address_space) {
    auto cfg_mem_vma = address_space
                           .MapBackingMemory(Memory::CONFIG_MEMORY_VADDR,
                                             reinterpret_cast<u8*>(&ConfigMem::config_mem),
                                             Memory::CONFIG_MEMORY_SIZE, MemoryState::Shared)
                           .Unwrap();
    address_space.Reprotect(cfg_mem_vma, VMAPermission::Read);

    auto shared_page_vma =
        address_space
            .MapBackingMemory(
                Memory::SHARED_PAGE_VADDR,
                reinterpret_cast<u8*>(
                    &Core::System::GetInstance().GetSharedPageHandler()->GetSharedPage()),
                Memory::SHARED_PAGE_SIZE, MemoryState::Shared)
            .Unwrap();
    address_space.Reprotect(shared_page_vma, VMAPermission::Read);
}

} // namespace Kernel
