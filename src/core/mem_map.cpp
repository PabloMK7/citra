// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <memory>
#include <utility>
#include <vector>

#include "common/common_types.h"
#include "common/logging/log.h"

#include "core/hle/config_mem.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/hle/result.h"
#include "core/hle/shared_page.h"
#include "core/mem_map.h"
#include "core/memory.h"
#include "core/memory_setup.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Memory {

namespace {

struct MemoryArea {
    u32 base;
    u32 size;
    const char* name;
};

// We don't declare the IO regions in here since its handled by other means.
static MemoryArea memory_areas[] = {
    {HEAP_VADDR,          HEAP_SIZE,              "Heap"},          // Application heap (main memory)
    {SHARED_MEMORY_VADDR, SHARED_MEMORY_SIZE,     "Shared Memory"}, // Shared memory
    {LINEAR_HEAP_VADDR,   LINEAR_HEAP_SIZE,       "Linear Heap"},   // Linear heap (main memory)
    {VRAM_VADDR,          VRAM_SIZE,              "VRAM"},          // Video memory (VRAM)
    {DSP_RAM_VADDR,       DSP_RAM_SIZE,           "DSP RAM"},       // DSP memory
    {TLS_AREA_VADDR,      TLS_AREA_SIZE,          "TLS Area"},      // TLS memory
};

/// Represents a block of memory mapped by ControlMemory/MapMemoryBlock
struct MemoryBlock {
    MemoryBlock() : handle(0), base_address(0), address(0), size(0), operation(0), permissions(0) {
    }
    u32 handle;
    u32 base_address;
    u32 address;
    u32 size;
    u32 operation;
    u32 permissions;

    const u32 GetVirtualAddress() const{
        return base_address + address;
    }
};

static std::map<u32, MemoryBlock> heap_map;
static std::map<u32, MemoryBlock> heap_linear_map;

}

u32 MapBlock_Heap(u32 size, u32 operation, u32 permissions) {
    MemoryBlock block;

    block.base_address  = HEAP_VADDR;
    block.size          = size;
    block.operation     = operation;
    block.permissions   = permissions;

    if (heap_map.size() > 0) {
        const MemoryBlock last_block = heap_map.rbegin()->second;
        block.address = last_block.address + last_block.size;
    }
    heap_map[block.GetVirtualAddress()] = block;

    return block.GetVirtualAddress();
}

u32 MapBlock_HeapLinear(u32 size, u32 operation, u32 permissions) {
    MemoryBlock block;

    block.base_address  = LINEAR_HEAP_VADDR;
    block.size          = size;
    block.operation     = operation;
    block.permissions   = permissions;

    if (heap_linear_map.size() > 0) {
        const MemoryBlock last_block = heap_linear_map.rbegin()->second;
        block.address = last_block.address + last_block.size;
    }
    heap_linear_map[block.GetVirtualAddress()] = block;

    return block.GetVirtualAddress();
}

PAddr VirtualToPhysicalAddress(const VAddr addr) {
    if (addr == 0) {
        return 0;
    } else if (addr >= VRAM_VADDR && addr < VRAM_VADDR_END) {
        return addr - VRAM_VADDR + VRAM_PADDR;
    } else if (addr >= LINEAR_HEAP_VADDR && addr < LINEAR_HEAP_VADDR_END) {
        return addr - LINEAR_HEAP_VADDR + FCRAM_PADDR;
    } else if (addr >= DSP_RAM_VADDR && addr < DSP_RAM_VADDR_END) {
        return addr - DSP_RAM_VADDR + DSP_RAM_PADDR;
    } else if (addr >= IO_AREA_VADDR && addr < IO_AREA_VADDR_END) {
        return addr - IO_AREA_VADDR + IO_AREA_PADDR;
    }

    LOG_ERROR(HW_Memory, "Unknown virtual address @ 0x%08x", addr);
    // To help with debugging, set bit on address so that it's obviously invalid.
    return addr | 0x80000000;
}

VAddr PhysicalToVirtualAddress(const PAddr addr) {
    if (addr == 0) {
        return 0;
    } else if (addr >= VRAM_PADDR && addr < VRAM_PADDR_END) {
        return addr - VRAM_PADDR + VRAM_VADDR;
    } else if (addr >= FCRAM_PADDR && addr < FCRAM_PADDR_END) {
        return addr - FCRAM_PADDR + LINEAR_HEAP_VADDR;
    } else if (addr >= DSP_RAM_PADDR && addr < DSP_RAM_PADDR_END) {
        return addr - DSP_RAM_PADDR + DSP_RAM_VADDR;
    } else if (addr >= IO_AREA_PADDR && addr < IO_AREA_PADDR_END) {
        return addr - IO_AREA_PADDR + IO_AREA_VADDR;
    }

    LOG_ERROR(HW_Memory, "Unknown physical address @ 0x%08x", addr);
    // To help with debugging, set bit on address so that it's obviously invalid.
    return addr | 0x80000000;
}

void Init() {
    InitMemoryMap();
    LOG_DEBUG(HW_Memory, "initialized OK");
}

void InitLegacyAddressSpace(Kernel::VMManager& address_space) {
    using namespace Kernel;

    for (MemoryArea& area : memory_areas) {
        auto block = std::make_shared<std::vector<u8>>(area.size);
        address_space.MapMemoryBlock(area.base, std::move(block), 0, area.size, MemoryState::Private).Unwrap();
    }

    auto cfg_mem_vma = address_space.MapBackingMemory(CONFIG_MEMORY_VADDR,
            (u8*)&ConfigMem::config_mem, CONFIG_MEMORY_SIZE, MemoryState::Shared).MoveFrom();
    address_space.Reprotect(cfg_mem_vma, VMAPermission::Read);

    auto shared_page_vma = address_space.MapBackingMemory(SHARED_PAGE_VADDR,
            (u8*)&SharedPage::shared_page, SHARED_PAGE_SIZE, MemoryState::Shared).MoveFrom();
    address_space.Reprotect(shared_page_vma, VMAPermission::Read);
}

void Shutdown() {
    heap_map.clear();
    heap_linear_map.clear();

    LOG_DEBUG(HW_Memory, "shutdown OK");
}

} // namespace
