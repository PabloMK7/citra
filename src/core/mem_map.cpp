// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>

#include "common/common_types.h"
#include "common/logging/log.h"

#include "core/hle/config_mem.h"
#include "core/hle/shared_page.h"
#include "core/mem_map.h"
#include "core/memory.h"
#include "core/memory_setup.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Memory {

u8* g_exefs_code;  ///< ExeFS:/.code is loaded here
u8* g_heap;        ///< Application heap (main memory)
u8* g_shared_mem;  ///< Shared memory
u8* g_heap_linear; ///< Linear heap
u8* g_vram;        ///< Video memory (VRAM) pointer
u8* g_dsp_mem;     ///< DSP memory
u8* g_tls_mem;     ///< TLS memory

namespace {

struct MemoryArea {
    u8** ptr;
    u32 base;
    u32 size;
};

// We don't declare the IO regions in here since its handled by other means.
static MemoryArea memory_areas[] = {
    {&g_exefs_code,  PROCESS_IMAGE_VADDR, PROCESS_IMAGE_MAX_SIZE},
    {&g_heap,        HEAP_VADDR,          HEAP_SIZE             },
    {&g_shared_mem,  SHARED_MEMORY_VADDR, SHARED_MEMORY_SIZE    },
    {&g_heap_linear, LINEAR_HEAP_VADDR,   LINEAR_HEAP_SIZE      },
    {&g_vram,        VRAM_VADDR,          VRAM_SIZE             },
    {&g_dsp_mem,     DSP_RAM_VADDR,       DSP_RAM_SIZE          },
    {&g_tls_mem,     TLS_AREA_VADDR,      TLS_AREA_SIZE         },
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

    for (MemoryArea& area : memory_areas) {
        *area.ptr = new u8[area.size];
        MapMemoryRegion(area.base, area.size, *area.ptr);
    }
    MapMemoryRegion(CONFIG_MEMORY_VADDR, CONFIG_MEMORY_SIZE, (u8*)&ConfigMem::config_mem);
    MapMemoryRegion(SHARED_PAGE_VADDR, SHARED_PAGE_SIZE, (u8*)&SharedPage::shared_page);

    LOG_DEBUG(HW_Memory, "initialized OK, RAM at %p", g_heap);
}

void Shutdown() {
    heap_map.clear();
    heap_linear_map.clear();

    for (MemoryArea& area : memory_areas) {
        delete[] *area.ptr;
        *area.ptr = nullptr;
    }

    LOG_DEBUG(HW_Memory, "shutdown OK");
}

} // namespace
