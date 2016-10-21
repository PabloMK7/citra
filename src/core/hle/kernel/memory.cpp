// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <memory>
#include <utility>
#include <vector>
#include "audio_core/audio_core.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/config_mem.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/hle/result.h"
#include "core/hle/shared_page.h"
#include "core/memory.h"
#include "core/memory_setup.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Kernel {

static MemoryRegionInfo memory_regions[3];

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
}

namespace Memory {

namespace {

struct MemoryArea {
    u32 base;
    u32 size;
    const char* name;
};

// We don't declare the IO regions in here since its handled by other means.
static MemoryArea memory_areas[] = {
    {VRAM_VADDR, VRAM_SIZE, "VRAM"}, // Video memory (VRAM)
};
}

void Init() {
    InitMemoryMap();
    LOG_DEBUG(HW_Memory, "initialized OK");
}

void InitLegacyAddressSpace(Kernel::VMManager& address_space) {
    using namespace Kernel;

    for (MemoryArea& area : memory_areas) {
        auto block = std::make_shared<std::vector<u8>>(area.size);
        address_space
            .MapMemoryBlock(area.base, std::move(block), 0, area.size, MemoryState::Private)
            .Unwrap();
    }

    auto cfg_mem_vma = address_space
                           .MapBackingMemory(CONFIG_MEMORY_VADDR, (u8*)&ConfigMem::config_mem,
                                             CONFIG_MEMORY_SIZE, MemoryState::Shared)
                           .MoveFrom();
    address_space.Reprotect(cfg_mem_vma, VMAPermission::Read);

    auto shared_page_vma = address_space
                               .MapBackingMemory(SHARED_PAGE_VADDR, (u8*)&SharedPage::shared_page,
                                                 SHARED_PAGE_SIZE, MemoryState::Shared)
                               .MoveFrom();
    address_space.Reprotect(shared_page_vma, VMAPermission::Read);

    AudioCore::AddAddressSpace(address_space);
}

} // namespace
