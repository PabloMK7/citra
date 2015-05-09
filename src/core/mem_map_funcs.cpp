// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/swap.h"

#include "core/mem_map.h"
#include "core/hw/hw.h"
#include "hle/config_mem.h"
#include "hle/shared_page.h"

namespace Memory {

static std::map<u32, MemoryBlock> heap_map;
static std::map<u32, MemoryBlock> heap_linear_map;

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

template <typename T>
inline void Read(T &var, const VAddr vaddr) {
    // TODO: Figure out the fastest order of tests for both read and write (they are probably different).
    // TODO: Make sure this represents the mirrors in a correct way.
    // Could just do a base-relative read, too.... TODO

    // Kernel memory command buffer
    if (vaddr >= TLS_AREA_VADDR && vaddr < TLS_AREA_VADDR_END) {
        var = *((const T*)&g_tls_mem[vaddr - TLS_AREA_VADDR]);

    // ExeFS:/.code is loaded here
    } else if ((vaddr >= PROCESS_IMAGE_VADDR)  && (vaddr < PROCESS_IMAGE_VADDR_END)) {
        var = *((const T*)&g_exefs_code[vaddr - PROCESS_IMAGE_VADDR]);

    // FCRAM - linear heap
    } else if ((vaddr >= LINEAR_HEAP_VADDR) && (vaddr < LINEAR_HEAP_VADDR_END)) {
        var = *((const T*)&g_heap_linear[vaddr - LINEAR_HEAP_VADDR]);

    // FCRAM - application heap
    } else if ((vaddr >= HEAP_VADDR)  && (vaddr < HEAP_VADDR_END)) {
        var = *((const T*)&g_heap[vaddr - HEAP_VADDR]);

    // Shared memory
    } else if ((vaddr >= SHARED_MEMORY_VADDR)  && (vaddr < SHARED_MEMORY_VADDR_END)) {
        var = *((const T*)&g_shared_mem[vaddr - SHARED_MEMORY_VADDR]);

    // Config memory
    } else if ((vaddr >= CONFIG_MEMORY_VADDR)  && (vaddr < CONFIG_MEMORY_VADDR_END)) {
        ConfigMem::Read<T>(var, vaddr);

    // Shared page
    } else if ((vaddr >= SHARED_PAGE_VADDR)  && (vaddr < SHARED_PAGE_VADDR_END)) {
        SharedPage::Read<T>(var, vaddr);

    // DSP memory
    } else if ((vaddr >= DSP_RAM_VADDR)  && (vaddr < DSP_RAM_VADDR_END)) {
        var = *((const T*)&g_dsp_mem[vaddr - DSP_RAM_VADDR]);

    // VRAM
    } else if ((vaddr >= VRAM_VADDR)  && (vaddr < VRAM_VADDR_END)) {
        var = *((const T*)&g_vram[vaddr - VRAM_VADDR]);

    } else {
        LOG_ERROR(HW_Memory, "unknown Read%lu @ 0x%08X", sizeof(var) * 8, vaddr);
    }
}

template <typename T>
inline void Write(const VAddr vaddr, const T data) {

    // Kernel memory command buffer
    if (vaddr >= TLS_AREA_VADDR && vaddr < TLS_AREA_VADDR_END) {
        *(T*)&g_tls_mem[vaddr - TLS_AREA_VADDR] = data;

    // ExeFS:/.code is loaded here
    } else if ((vaddr >= PROCESS_IMAGE_VADDR)  && (vaddr < PROCESS_IMAGE_VADDR_END)) {
        *(T*)&g_exefs_code[vaddr - PROCESS_IMAGE_VADDR] = data;

    // FCRAM - linear heap
    } else if ((vaddr >= LINEAR_HEAP_VADDR)  && (vaddr < LINEAR_HEAP_VADDR_END)) {
        *(T*)&g_heap_linear[vaddr - LINEAR_HEAP_VADDR] = data;

    // FCRAM - application heap
    } else if ((vaddr >= HEAP_VADDR)  && (vaddr < HEAP_VADDR_END)) {
        *(T*)&g_heap[vaddr - HEAP_VADDR] = data;

    // Shared memory
    } else if ((vaddr >= SHARED_MEMORY_VADDR)  && (vaddr < SHARED_MEMORY_VADDR_END)) {
        *(T*)&g_shared_mem[vaddr - SHARED_MEMORY_VADDR] = data;

    // VRAM
    } else if ((vaddr >= VRAM_VADDR)  && (vaddr < VRAM_VADDR_END)) {
        *(T*)&g_vram[vaddr - VRAM_VADDR] = data;

    // DSP memory
    } else if ((vaddr >= DSP_RAM_VADDR)  && (vaddr < DSP_RAM_VADDR_END)) {
        *(T*)&g_dsp_mem[vaddr - DSP_RAM_VADDR] = data;

    //} else if ((vaddr & 0xFFFF0000) == 0x1FF80000) {
    //    ASSERT_MSG(MEMMAP, false, "umimplemented write to Configuration Memory");
    //} else if ((vaddr & 0xFFFFF000) == 0x1FF81000) {
    //    ASSERT_MSG(MEMMAP, false, "umimplemented write to shared page");

    // Error out...
    } else {
        LOG_ERROR(HW_Memory, "unknown Write%lu 0x%08X @ 0x%08X", sizeof(data) * 8, (u32)data, vaddr);
    }
}

u8 *GetPointer(const VAddr vaddr) {
    // Kernel memory command buffer
    if (vaddr >= TLS_AREA_VADDR && vaddr < TLS_AREA_VADDR_END) {
        return g_tls_mem + (vaddr - TLS_AREA_VADDR);

    // ExeFS:/.code is loaded here
    } else if ((vaddr >= PROCESS_IMAGE_VADDR)  && (vaddr < PROCESS_IMAGE_VADDR_END)) {
        return g_exefs_code + (vaddr - PROCESS_IMAGE_VADDR);

    // FCRAM - linear heap
    } else if ((vaddr >= LINEAR_HEAP_VADDR)  && (vaddr < LINEAR_HEAP_VADDR_END)) {
        return g_heap_linear + (vaddr - LINEAR_HEAP_VADDR);

    // FCRAM - application heap
    } else if ((vaddr >= HEAP_VADDR)  && (vaddr < HEAP_VADDR_END)) {
        return g_heap + (vaddr - HEAP_VADDR);

    // Shared memory
    } else if ((vaddr >= SHARED_MEMORY_VADDR)  && (vaddr < SHARED_MEMORY_VADDR_END)) {
        return g_shared_mem + (vaddr - SHARED_MEMORY_VADDR);

    // VRAM
    } else if ((vaddr >= VRAM_VADDR)  && (vaddr < VRAM_VADDR_END)) {
        return g_vram + (vaddr - VRAM_VADDR);

    } else {
        LOG_ERROR(HW_Memory, "unknown GetPointer @ 0x%08x", vaddr);
        return 0;
    }
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

void MemBlock_Init() {
}

void MemBlock_Shutdown() {
    heap_map.clear();
    heap_linear_map.clear();
}

u8 Read8(const VAddr addr) {
    u8 data = 0;
    Read<u8>(data, addr);
    return data;
}

u16 Read16(const VAddr addr) {
    u16_le data = 0;
    Read<u16_le>(data, addr);
    return (u16)data;
}

u32 Read32(const VAddr addr) {
    u32_le data = 0;
    Read<u32_le>(data, addr);
    return (u32)data;
}

u64 Read64(const VAddr addr) {
    u64_le data = 0;
    Read<u64_le>(data, addr);
    return (u64)data;
}

u32 Read8_ZX(const VAddr addr) {
    return (u32)Read8(addr);
}

u32 Read16_ZX(const VAddr addr) {
    return (u32)Read16(addr);
}

void Write8(const VAddr addr, const u8 data) {
    Write<u8>(addr, data);
}

void Write16(const VAddr addr, const u16 data) {
    Write<u16_le>(addr, data);
}

void Write32(const VAddr addr, const u32 data) {
    Write<u32_le>(addr, data);
}

void Write64(const VAddr addr, const u64 data) {
    Write<u64_le>(addr, data);
}

void WriteBlock(const VAddr addr, const u8* data, const size_t size) {
    u32 offset = 0;
    while (offset < (size & ~3)) {
        Write32(addr + offset, *(u32*)&data[offset]);
        offset += 4;
    }

    if (size & 2) {
        Write16(addr + offset, *(u16*)&data[offset]);
        offset += 2;
    }

    if (size & 1)
        Write8(addr + offset, data[offset]);
}

} // namespace
