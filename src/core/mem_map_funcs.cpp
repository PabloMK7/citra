// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common.h"

#include "core/mem_map.h"
#include "core/hw/hw.h"
#include "hle/hle.h"

namespace Memory {

/// Convert a physical address to virtual address
u32 _AddressPhysicalToVirtual(const u32 addr) {
    // Our memory interface read/write functions assume virtual addresses. Put any physical address 
    // to virtual address translations here. This is obviously quite hacky... But we're not doing 
    // any MMU emulation yet or anything
    if ((addr >= FCRAM_PADDR) && (addr < (FCRAM_PADDR_END))) {
        return (addr & FCRAM_MASK) | FCRAM_VADDR;
    }
    return addr;
}

template <typename T>
inline void _Read(T &var, const u32 addr) {
    // TODO: Figure out the fastest order of tests for both read and write (they are probably different).
    // TODO: Make sure this represents the mirrors in a correct way.
    // Could just do a base-relative read, too.... TODO

    const u32 vaddr = _AddressPhysicalToVirtual(addr);
    
    // Memory allocated for HLE use that can be addressed from the emulated application
    // The primary use of this is sharing a commandbuffer between the HLE OS (syscore) and the LLE
    // core running the user application (appcore)
    if (vaddr >= HLE::CMD_BUFFER_ADDR && vaddr < HLE::CMD_BUFFER_ADDR_END) {
        HLE::Read<T>(var, vaddr);

    // Hardware I/O register reads
    // 0x10XXXXXX- is physical address space, 0x1EXXXXXX is virtual address space
    } else if ((vaddr & 0xFF000000) == 0x10000000 || (vaddr & 0xFF000000) == 0x1E000000) {
        HW::Read<T>(var, vaddr);

    // FCRAM - application heap
    } else if ((vaddr > HEAP_VADDR)  && (vaddr < HEAP_VADDR_END)) {
        var = *((const T*)&g_heap[vaddr & HEAP_MASK]);

    /*else if ((vaddr & 0x3F800000) == 0x04000000) {
        var = *((const T*)&m_pVRAM[vaddr & VRAM_MASK]);*/

    } else {
        //_assert_msg_(MEMMAP, false, "unknown Read%d @ 0x%08X", sizeof(var) * 8, vaddr);
    }
}

template <typename T>
inline void _Write(u32 addr, const T data) {
    u32 vaddr = _AddressPhysicalToVirtual(addr);
    
    // Memory allocated for HLE use that can be addressed from the emulated application
    // The primary use of this is sharing a commandbuffer between the HLE OS (syscore) and the LLE
    // core running the user application (appcore)
    if (vaddr >= HLE::CMD_BUFFER_ADDR && vaddr < HLE::CMD_BUFFER_ADDR_END) {
        HLE::Write<T>(vaddr, data);

    // Hardware I/O register writes
    // 0x10XXXXXX- is physical address space, 0x1EXXXXXX is virtual address space
    } else if ((vaddr & 0xFF000000) == 0x10000000 || (vaddr & 0xFF000000) == 0x1E000000) {
        HW::Write<T>(vaddr, data);

    // FCRAM - GSP heap
    //} else if ((vaddr > HEAP_GSP_VADDR)  && (vaddr < HEAP_VADDR_GSP_END)) {
    //    *(T*)&g_heap_gsp[vaddr & FCRAM_MASK] = data;

    // FCRAM - application heap
    } else if ((vaddr > HEAP_VADDR)  && (vaddr < HEAP_VADDR_END)) {
        *(T*)&g_heap[vaddr & HEAP_MASK] = data;

    } else if ((vaddr & 0xFF000000) == 0x14000000) {
        _assert_msg_(MEMMAP, false, "umimplemented write to GSP heap");
    } else if ((vaddr & 0xFFF00000) == 0x1EC00000) {
        _assert_msg_(MEMMAP, false, "umimplemented write to IO registers");
    } else if ((vaddr & 0xFF000000) == 0x1F000000) {
        _assert_msg_(MEMMAP, false, "umimplemented write to VRAM");
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
    const u32 vaddr = _AddressPhysicalToVirtual(addr);

    // FCRAM - application heap
    if ((vaddr > HEAP_VADDR)  && (vaddr < HEAP_VADDR_END)) {
        return g_heap + (vaddr & HEAP_MASK);

    } else {
        ERROR_LOG(MEMMAP, "Unknown GetPointer @ 0x%08x", vaddr);
        return 0;
    }
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
