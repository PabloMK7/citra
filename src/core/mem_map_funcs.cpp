// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <map>

#include "common/common.h"

#include "core/mem_map.h"
#include "core/hw/hw.h"
#include "hle/config_mem.h"

namespace Memory {

static std::map<u32, MemoryBlock> heap_map;
static std::map<u32, MemoryBlock> heap_linear_map;
static std::map<u32, MemoryBlock> shared_map;

/// Convert a physical address to virtual address
VAddr PhysicalToVirtualAddress(const PAddr addr) {
    // Our memory interface read/write functions assume virtual addresses. Put any physical address
    // to virtual address translations here. This is quite hacky, but necessary until we implement
    // proper MMU emulation.
    // TODO: Screw it, I'll let bunnei figure out how to do this properly.
    if ((addr >= VRAM_PADDR) && (addr < VRAM_PADDR_END)) {
        return addr - VRAM_PADDR + VRAM_VADDR;
    }else if ((addr >= FCRAM_PADDR) && (addr < FCRAM_PADDR_END)) {
        return addr - FCRAM_PADDR + FCRAM_VADDR;
    }

    ERROR_LOG(MEMMAP, "Unknown physical address @ 0x%08x", addr);
    return addr;
}

/// Convert a physical address to virtual address
PAddr VirtualToPhysicalAddress(const VAddr addr) {
    // Our memory interface read/write functions assume virtual addresses. Put any physical address
    // to virtual address translations here. This is quite hacky, but necessary until we implement
    // proper MMU emulation.
    // TODO: Screw it, I'll let bunnei figure out how to do this properly.
    if ((addr >= VRAM_VADDR) && (addr < VRAM_VADDR_END)) {
        return addr - 0x07000000;
    } else if ((addr >= FCRAM_VADDR) && (addr < FCRAM_VADDR_END)) {
        return addr - FCRAM_VADDR + FCRAM_PADDR;
    }

    ERROR_LOG(MEMMAP, "Unknown virtual address @ 0x%08x", addr);
    return addr;
}

template <typename T>
inline void Read(T &var, const VAddr vaddr) {
    // TODO: Figure out the fastest order of tests for both read and write (they are probably different).
    // TODO: Make sure this represents the mirrors in a correct way.
    // Could just do a base-relative read, too.... TODO

    // Kernel memory command buffer
    if (vaddr >= KERNEL_MEMORY_VADDR && vaddr < KERNEL_MEMORY_VADDR_END) {
        var = *((const T*)&g_kernel_mem[vaddr - KERNEL_MEMORY_VADDR]);

    // Hardware I/O register reads
    // 0x10XXXXXX- is physical address space, 0x1EXXXXXX is virtual address space
    } else if ((vaddr >= HARDWARE_IO_VADDR) && (vaddr < HARDWARE_IO_VADDR_END)) {
        HW::Read<T>(var, vaddr);

    // ExeFS:/.code is loaded here
    } else if ((vaddr >= EXEFS_CODE_VADDR)  && (vaddr < EXEFS_CODE_VADDR_END)) {
        var = *((const T*)&g_exefs_code[vaddr - EXEFS_CODE_VADDR]);

    // FCRAM - linear heap
    } else if ((vaddr >= HEAP_LINEAR_VADDR) && (vaddr < HEAP_LINEAR_VADDR_END)) {
        var = *((const T*)&g_heap_linear[vaddr - HEAP_LINEAR_VADDR]);

    // FCRAM - application heap
    } else if ((vaddr >= HEAP_VADDR)  && (vaddr < HEAP_VADDR_END)) {
        var = *((const T*)&g_heap[vaddr - HEAP_VADDR]);

    // Shared memory
    } else if ((vaddr >= SHARED_MEMORY_VADDR)  && (vaddr < SHARED_MEMORY_VADDR_END)) {
        var = *((const T*)&g_shared_mem[vaddr - SHARED_MEMORY_VADDR]);

    // System memory
    } else if ((vaddr >= SYSTEM_MEMORY_VADDR)  && (vaddr < SYSTEM_MEMORY_VADDR_END)) {
        var = *((const T*)&g_system_mem[vaddr - SYSTEM_MEMORY_VADDR]);

    // Config memory
    } else if ((vaddr >= CONFIG_MEMORY_VADDR)  && (vaddr < CONFIG_MEMORY_VADDR_END)) {
        ConfigMem::Read<T>(var, vaddr);

    // VRAM
    } else if ((vaddr >= VRAM_VADDR)  && (vaddr < VRAM_VADDR_END)) {
        var = *((const T*)&g_vram[vaddr - VRAM_VADDR]);

    } else {
        ERROR_LOG(MEMMAP, "unknown Read%lu @ 0x%08X", sizeof(var) * 8, vaddr);
    }
}

template <typename T>
inline void Write(const VAddr vaddr, const T data) {

    // Kernel memory command buffer
    if (vaddr >= KERNEL_MEMORY_VADDR && vaddr < KERNEL_MEMORY_VADDR_END) {
        *(T*)&g_kernel_mem[vaddr - KERNEL_MEMORY_VADDR] = data;

    // Hardware I/O register writes
    // 0x10XXXXXX- is physical address space, 0x1EXXXXXX is virtual address space
    } else if ((vaddr >= HARDWARE_IO_VADDR) && (vaddr < HARDWARE_IO_VADDR_END)) {
        HW::Write<T>(vaddr, data);

    // ExeFS:/.code is loaded here
    } else if ((vaddr >= EXEFS_CODE_VADDR)  && (vaddr < EXEFS_CODE_VADDR_END)) {
        *(T*)&g_exefs_code[vaddr - EXEFS_CODE_VADDR] = data;

    // FCRAM - linear heap
    } else if ((vaddr >= HEAP_LINEAR_VADDR)  && (vaddr < HEAP_LINEAR_VADDR_END)) {
        *(T*)&g_heap_linear[vaddr - HEAP_LINEAR_VADDR] = data;

    // FCRAM - application heap
    } else if ((vaddr >= HEAP_VADDR)  && (vaddr < HEAP_VADDR_END)) {
        *(T*)&g_heap[vaddr - HEAP_VADDR] = data;

    // Shared memory
    } else if ((vaddr >= SHARED_MEMORY_VADDR)  && (vaddr < SHARED_MEMORY_VADDR_END)) {
        *(T*)&g_shared_mem[vaddr - SHARED_MEMORY_VADDR] = data;

    // System memory
    } else if ((vaddr >= SYSTEM_MEMORY_VADDR)  && (vaddr < SYSTEM_MEMORY_VADDR_END)) {
        *(T*)&g_system_mem[vaddr - SYSTEM_MEMORY_VADDR] = data;

    // VRAM
    } else if ((vaddr >= VRAM_VADDR)  && (vaddr < VRAM_VADDR_END)) {
        *(T*)&g_vram[vaddr - VRAM_VADDR] = data;

    //} else if ((vaddr & 0xFFF00000) == 0x1FF00000) {
    //    _assert_msg_(MEMMAP, false, "umimplemented write to DSP memory");
    //} else if ((vaddr & 0xFFFF0000) == 0x1FF80000) {
    //    _assert_msg_(MEMMAP, false, "umimplemented write to Configuration Memory");
    //} else if ((vaddr & 0xFFFFF000) == 0x1FF81000) {
    //    _assert_msg_(MEMMAP, false, "umimplemented write to shared page");

    // Error out...
    } else {
        ERROR_LOG(MEMMAP, "unknown Write%lu 0x%08X @ 0x%08X", sizeof(data) * 8, (u32)data, vaddr);
    }
}

u8 *GetPointer(const VAddr vaddr) {
    // Kernel memory command buffer
    if (vaddr >= KERNEL_MEMORY_VADDR && vaddr < KERNEL_MEMORY_VADDR_END) {
        return g_kernel_mem + (vaddr - KERNEL_MEMORY_VADDR);

    // ExeFS:/.code is loaded here
    } else if ((vaddr >= EXEFS_CODE_VADDR)  && (vaddr < EXEFS_CODE_VADDR_END)) {
        return g_exefs_code + (vaddr - EXEFS_CODE_VADDR);

    // FCRAM - linear heap
    } else if ((vaddr >= HEAP_LINEAR_VADDR)  && (vaddr < HEAP_LINEAR_VADDR_END)) {
        return g_heap_linear + (vaddr - HEAP_LINEAR_VADDR);

    // FCRAM - application heap
    } else if ((vaddr >= HEAP_VADDR)  && (vaddr < HEAP_VADDR_END)) {
        return g_heap + (vaddr - HEAP_VADDR);

    // Shared memory
    } else if ((vaddr >= SHARED_MEMORY_VADDR)  && (vaddr < SHARED_MEMORY_VADDR_END)) {
        return g_shared_mem + (vaddr - SHARED_MEMORY_VADDR);

    // System memory
    } else if ((vaddr >= SYSTEM_MEMORY_VADDR)  && (vaddr < SYSTEM_MEMORY_VADDR_END)) {
        return g_system_mem + (vaddr - SYSTEM_MEMORY_VADDR);

    // VRAM
    } else if ((vaddr >= VRAM_VADDR)  && (vaddr < VRAM_VADDR_END)) {
        return g_vram + (vaddr - VRAM_VADDR);

    } else {
        ERROR_LOG(MEMMAP, "unknown GetPointer @ 0x%08x", vaddr);
        return 0;
    }
}

/**
 * Maps a block of memory on the heap
 * @param size Size of block in bytes
 * @param operation Memory map operation type
 * @param flags Memory allocation flags
 */
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

/**
 * Maps a block of memory on the linear heap
 * @param size Size of block in bytes
 * @param operation Memory map operation type
 * @param flags Memory allocation flags
 */
u32 MapBlock_HeapLinear(u32 size, u32 operation, u32 permissions) {
    MemoryBlock block;

    block.base_address  = HEAP_LINEAR_VADDR;
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

u8 Read8(const VAddr addr) {
    u8 data = 0;
    Read<u8>(data, addr);
    return data;
}

u16 Read16(const VAddr addr) {
    u16_le data = 0;
    Read<u16_le>(data, addr);

    // Check for 16-bit unaligned memory reads...
    if (addr & 1) {
        // TODO(bunnei): Implement 16-bit unaligned memory reads
        ERROR_LOG(MEMMAP, "16-bit unaligned memory reads are not implemented!");
    }

    return (u16)data;
}

u32 Read32(const VAddr addr) {
    u32_le data = 0;
    Read<u32_le>(data, addr);

    // Check for 32-bit unaligned memory reads...
    if (addr & 3) {
        // ARM allows for unaligned memory reads, however older ARM architectures read out memory
        // from unaligned addresses in a shifted way. Our ARM CPU core (SkyEye) corrects for this,
        // so therefore expects the memory to be read out in this manner.
        // TODO(bunnei): Determine if this is necessary - perhaps it is OK to remove this from both
        // SkyEye and here?
        int shift = (addr & 3) * 8;
        data = (data << shift) | (data >> (32 - shift));
    }
    return (u32)data;
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
