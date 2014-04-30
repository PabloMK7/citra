// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <map>

#include "common/common.h"

#include "core/mem_map.h"
#include "core/hw/hw.h"
#include "hle/hle.h"

namespace Memory {

std::map<u32, MemoryBlock> g_heap_map;
std::map<u32, MemoryBlock> g_heap_gsp_map;
std::map<u32, MemoryBlock> g_shared_map;

/// Convert a physical address (or firmware-specific virtual address) to primary virtual address
u32 _VirtualAddress(const u32 addr) {
    // Our memory interface read/write functions assume virtual addresses. Put any physical address 
    // to virtual address translations here. This is obviously quite hacky... But we're not doing 
    // any MMU emulation yet or anything
    if ((addr >= FCRAM_PADDR) && (addr < FCRAM_PADDR_END)) {
        return VirtualAddressFromPhysical_FCRAM(addr);

    // Virtual address mapping FW0B
    } else if ((addr >= FCRAM_VADDR_FW0B) && (addr < FCRAM_VADDR_FW0B_END)) {
        return VirtualAddressFromPhysical_FCRAM(addr);

    // Hardware IO
    // TODO(bunnei): FixMe
    // This isn't going to work... The physical address of HARDWARE_IO conflicts with the virtual 
    // address of shared memory.
    //} else if ((addr >= HARDWARE_IO_PADDR) && (addr < HARDWARE_IO_PADDR_END)) {
    //    return (addr + 0x0EB00000);

    }
    return addr;
}

template <typename T>
inline void _Read(T &var, const u32 addr) {
    // TODO: Figure out the fastest order of tests for both read and write (they are probably different).
    // TODO: Make sure this represents the mirrors in a correct way.
    // Could just do a base-relative read, too.... TODO

    const u32 vaddr = _VirtualAddress(addr);
    
    // Memory allocated for HLE use that can be addressed from the emulated application
    // The primary use of this is sharing a commandbuffer between the HLE OS (syscore) and the LLE
    // core running the user application (appcore)
    if (vaddr >= HLE::CMD_BUFFER_ADDR && vaddr < HLE::CMD_BUFFER_ADDR_END) {
        HLE::Read<T>(var, vaddr);

    // Hardware I/O register reads
    // 0x10XXXXXX- is physical address space, 0x1EXXXXXX is virtual address space
    } else if ((vaddr >= HARDWARE_IO_VADDR) && (vaddr < HARDWARE_IO_VADDR_END)) {
        HW::Read<T>(var, vaddr);

    // FCRAM - GSP heap
    } else if ((vaddr >= HEAP_GSP_VADDR) && (vaddr < HEAP_GSP_VADDR_END)) {
        var = *((const T*)&g_heap_gsp[vaddr & HEAP_GSP_MASK]);

    // FCRAM - application heap
    } else if ((vaddr >= HEAP_VADDR)  && (vaddr < HEAP_VADDR_END)) {
        var = *((const T*)&g_heap[vaddr & HEAP_MASK]);

    // Shared memory
    } else if ((vaddr >= SHARED_MEMORY_VADDR)  && (vaddr < SHARED_MEMORY_VADDR_END)) {
        var = *((const T*)&g_shared_mem[vaddr & SHARED_MEMORY_MASK]);

    // VRAM
    } else if ((vaddr >= VRAM_VADDR)  && (vaddr < VRAM_VADDR_END)) {
        var = *((const T*)&g_vram[vaddr & VRAM_MASK]);

    } else {
        //_assert_msg_(MEMMAP, false, "unknown Read%d @ 0x%08X", sizeof(var) * 8, vaddr);
    }
}

template <typename T>
inline void _Write(u32 addr, const T data) {
    u32 vaddr = _VirtualAddress(addr);
    
    // Memory allocated for HLE use that can be addressed from the emulated application
    // The primary use of this is sharing a commandbuffer between the HLE OS (syscore) and the LLE
    // core running the user application (appcore)
    if (vaddr >= HLE::CMD_BUFFER_ADDR && vaddr < HLE::CMD_BUFFER_ADDR_END) {
        HLE::Write<T>(vaddr, data);

    // Hardware I/O register writes
    // 0x10XXXXXX- is physical address space, 0x1EXXXXXX is virtual address space
    } else if ((vaddr >= HARDWARE_IO_VADDR) && (vaddr < HARDWARE_IO_VADDR_END)) {
        HW::Write<T>(vaddr, data);

    // FCRAM - GSP heap
    } else if ((vaddr >= HEAP_GSP_VADDR)  && (vaddr < HEAP_GSP_VADDR_END)) {
        *(T*)&g_heap_gsp[vaddr & HEAP_GSP_MASK] = data;

    // FCRAM - application heap
    } else if ((vaddr >= HEAP_VADDR)  && (vaddr < HEAP_VADDR_END)) {
        *(T*)&g_heap[vaddr & HEAP_MASK] = data;

    // Shared memory
    } else if ((vaddr >= SHARED_MEMORY_VADDR)  && (vaddr < SHARED_MEMORY_VADDR_END)) {
        *(T*)&g_shared_mem[vaddr & SHARED_MEMORY_MASK] = data;

    // VRAM
    } else if ((vaddr >= VRAM_VADDR)  && (vaddr < VRAM_VADDR_END)) {
        *(T*)&g_vram[vaddr & VRAM_MASK] = data;

    } else if ((vaddr & 0xFFF00000) == 0x1FF00000) {
        _assert_msg_(MEMMAP, false, "umimplemented write to DSP memory");
    } else if ((vaddr & 0xFFFF0000) == 0x1FF80000) {
        _assert_msg_(MEMMAP, false, "umimplemented write to Configuration Memory");
    } else if ((vaddr & 0xFFFFF000) == 0x1FF81000) {
        _assert_msg_(MEMMAP, false, "umimplemented write to shared page");
    
    // Error out...
    } else {
        _assert_msg_(MEMMAP, false, "unknown Write%d 0x%08X @ 0x%08X", sizeof(data) * 8,
            data, vaddr);
    }
}

u8 *GetPointer(const u32 addr) {
    const u32 vaddr = _VirtualAddress(addr);

    // FCRAM - GSP heap
    if ((vaddr >= HEAP_GSP_VADDR)  && (vaddr < HEAP_GSP_VADDR_END)) {
        return g_heap_gsp + (vaddr & HEAP_GSP_MASK);

    // FCRAM - application heap
    } else if ((vaddr >= HEAP_VADDR)  && (vaddr < HEAP_VADDR_END)) {
        return g_heap + (vaddr & HEAP_MASK);

    // Shared memory
    } else if ((vaddr > SHARED_MEMORY_VADDR)  && (vaddr < SHARED_MEMORY_VADDR_END)) {
        return g_shared_mem + (vaddr & SHARED_MEMORY_MASK);

    // VRAM
    } else if ((vaddr > VRAM_VADDR)  && (vaddr < VRAM_VADDR_END)) {
        return g_vram + (vaddr & VRAM_MASK);

    } else {
        ERROR_LOG(MEMMAP, "unknown GetPointer @ 0x%08x", vaddr);
        return 0;
    }
}

/**
 * Maps a block of memory in shared memory
 * @param handle Handle to map memory block for
 * @param addr Address to map memory block to
 * @param permissions Memory map permissions
 */
u32 MapBlock_Shared(u32 handle, u32 addr,u32 permissions) {
    MemoryBlock block;
    
    block.handle        = handle;
    block.base_address  = addr;
    block.permissions   = permissions;
    
    if (g_shared_map.size() > 0) {
        const MemoryBlock last_block = g_shared_map.rbegin()->second;
        block.address = last_block.address + last_block.size;
    }
    g_shared_map[block.GetVirtualAddress()] = block;

    return block.GetVirtualAddress();
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
    
    if (g_heap_map.size() > 0) {
        const MemoryBlock last_block = g_heap_map.rbegin()->second;
        block.address = last_block.address + last_block.size;
    }
    g_heap_map[block.GetVirtualAddress()] = block;

    return block.GetVirtualAddress();
}

/**
 * Maps a block of memory on the GSP heap
 * @param size Size of block in bytes
 * @param operation Memory map operation type
 * @param flags Memory allocation flags
 */
u32 MapBlock_HeapGSP(u32 size, u32 operation, u32 permissions) {
    MemoryBlock block;
    
    block.base_address  = HEAP_GSP_VADDR;
    block.size          = size;
    block.operation     = operation;
    block.permissions   = permissions;
    
    if (g_heap_gsp_map.size() > 0) {
        const MemoryBlock last_block = g_heap_gsp_map.rbegin()->second;
        block.address = last_block.address + last_block.size;
    }
    g_heap_gsp_map[block.GetVirtualAddress()] = block;

    return block.GetVirtualAddress();
}

u8 Read8(const u32 addr) {
    u8 _var = 0;
    _Read<u8>(_var, addr);
    return (u8)_var;
}

u16 Read16(const u32 addr) {
    u16_le _var = 0;
    _Read<u16_le>(_var, addr);
    return (u16)_var;
}

u32 Read32(const u32 addr) {
    u32_le _var = 0;
    _Read<u32_le>(_var, addr);
    return _var;
}

u64 Read64(const u32 addr) {
    u64_le _var = 0;
    _Read<u64_le>(_var, addr);
    return _var;
}

u32 Read8_ZX(const u32 addr) {
    return (u32)Read8(addr);
}

u32 Read16_ZX(const u32 addr) {
    return (u32)Read16(addr);
}

void Write8(const u32 addr, const u8 data) {
    _Write<u8>(addr, data);
}

void Write16(const u32 addr, const u16 data) {
    _Write<u16_le>(addr, data);
}

void Write32(const u32 addr, const u32 data) {
    _Write<u32_le>(addr, data);
}

void Write64(const u32 addr, const u64 data) {
    _Write<u64_le>(addr, data);
}

} // namespace
