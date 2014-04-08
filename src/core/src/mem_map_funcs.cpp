// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common.h"

#include "mem_map.h"
#include "hw/hw.h"

namespace Memory {

template <typename T>
inline void _Read(T &var, const u32 addr) {
    // TODO: Figure out the fastest order of tests for both read and write (they are probably different).
    // TODO: Make sure this represents the mirrors in a correct way.
    // Could just do a base-relative read, too.... TODO

    // Hardware I/O register reads
    // 0x10XXXXXX- is physical address space, 0x1EXXXXXX is virtual address space
    if ((addr & 0xFF000000) == 0x10000000 || (addr & 0xFF000000) == 0x1E000000) {
        HW::Read<T>(var, addr);

    // FCRAM virtual address reads
    } else if ((addr & 0x3E000000) == 0x08000000) {
        var = *((const T*)&g_fcram[addr & MEM_FCRAM_MASK]);

    // Scratchpad memory
    } else if (addr > MEM_SCRATCHPAD_VADDR && addr <= (MEM_SCRATCHPAD_VADDR + MEM_SCRATCHPAD_SIZE)) {
        var = *((const T*)&g_scratchpad[addr & MEM_SCRATCHPAD_MASK]);
 
    /*else if ((addr & 0x3F800000) == 0x04000000) {
        var = *((const T*)&m_pVRAM[addr & VRAM_MASK]);
    }*/

    // HACK(bunnei): There is no layer yet to translate virtual addresses to physical addresses. 
    // Until we progress far enough along, we'll accept all physical address reads here. I think 
    // that this is typically a corner-case from usermode software unless they are trying to do 
    // bare-metal things (e.g. early 3DS homebrew writes directly to the FB @ 0x20184E60, etc.
    } else if (((addr & 0xF0000000) == MEM_FCRAM_PADDR) && (addr < (MEM_FCRAM_PADDR_END))) {
        var = *((const T*)&g_fcram[addr & MEM_FCRAM_MASK]);

    } else {
        _assert_msg_(MEMMAP, false, "unknown memory read");
    }
}

template <typename T>
inline void _Write(u32 addr, const T data) {
    
    // Hardware I/O register writes
    // 0x10XXXXXX- is physical address space, 0x1EXXXXXX is virtual address space
    if ((addr & 0xFF000000) == 0x10000000 || (addr & 0xFF000000) == 0x1E000000) {
        HW::Write<const T>(addr, data);
    
    // ExeFS:/.code is loaded here:
    } else if ((addr & 0xFFF00000) == 0x00100000) {
        // TODO(ShizZy): This is dumb... handle correctly. From 3DBrew:
        // http://3dbrew.org/wiki/Memory_layout#ARM11_User-land_memory_regions
        // The ExeFS:/.code is loaded here, executables must be loaded to the 0x00100000 region when
        // the exheader "special memory" flag is clear. The 0x03F00000-byte size restriction only 
        // applies when this flag is clear. Executables are usually loaded to 0x14000000 when the 
        // exheader "special memory" flag is set, however this address can be arbitrary.
        *(T*)&g_fcram[addr & MEM_FCRAM_MASK] = data;

    // Scratchpad memory
    } else if (addr > MEM_SCRATCHPAD_VADDR && addr <= (MEM_SCRATCHPAD_VADDR + MEM_SCRATCHPAD_SIZE)) {
        *(T*)&g_scratchpad[addr & MEM_SCRATCHPAD_MASK] = data;

    // Heap mapped by ControlMemory:
    } else if ((addr & 0x3E000000) == 0x08000000) {
        // TODO(ShizZy): Writes to this virtual address should be put in physical memory at FCRAM + GSP
        // heap size... the following is writing to FCRAM + 0, which is actually supposed to be the 
        // application's GSP heap
        *(T*)&g_fcram[addr & MEM_FCRAM_MASK] = data;

    } else if ((addr & 0xFF000000) == 0x14000000) {
        _assert_msg_(MEMMAP, false, "umimplemented write to GSP heap");
    } else if ((addr & 0xFFF00000) == 0x1EC00000) {
        _assert_msg_(MEMMAP, false, "umimplemented write to IO registers");
    } else if ((addr & 0xFF000000) == 0x1F000000) {
        _assert_msg_(MEMMAP, false, "umimplemented write to VRAM");
    } else if ((addr & 0xFFF00000) == 0x1FF00000) {
        _assert_msg_(MEMMAP, false, "umimplemented write to DSP memory");
    } else if ((addr & 0xFFFF0000) == 0x1FF80000) {
        _assert_msg_(MEMMAP, false, "umimplemented write to Configuration Memory");
    } else if ((addr & 0xFFFFF000) == 0x1FF81000) {
        _assert_msg_(MEMMAP, false, "umimplemented write to shared page");
    
    // HACK(bunnei): There is no layer yet to translate virtual addresses to physical addresses. 
    // Until we progress far enough along, we'll accept all physical address writes here. I think 
    // that this is typically a corner-case from usermode software unless they are trying to do 
    // bare-metal things (e.g. early 3DS homebrew writes directly to the FB @ 0x20184E60, etc.
    } else if (((addr & 0xF0000000) == MEM_FCRAM_PADDR) && (addr < (MEM_FCRAM_PADDR_END))) {
        *(T*)&g_fcram[addr & MEM_FCRAM_MASK] = data;

    // Error out...
    } else {
        _assert_msg_(MEMMAP, false, "unknown memory write");
    }
}

bool IsValidAddress(const u32 addr) {
    if ((addr & 0x3E000000) == 0x08000000) {
        return true;
    } else if ((addr & 0x3F800000) == 0x04000000) {
        return true;
    } else if ((addr & 0xBFFF0000) == 0x00010000) {
        return true;
    } else if ((addr & 0x3F000000) >= 0x08000000 && (addr & 0x3F000000) < 0x08000000 + MEM_FCRAM_MASK) {
        return true;
    } else {
        return false;
    }
}

u8 *GetPointer(const u32 addr) {
    // TODO(bunnei): Just a stub for now... ImplementMe!
    if ((addr & 0x3E000000) == 0x08000000) {
        return g_fcram + (addr & MEM_FCRAM_MASK);

    // HACK(bunnei): There is no layer yet to translate virtual addresses to physical addresses. 
    // Until we progress far enough along, we'll accept all physical address reads here. I think 
    // that this is typically a corner-case from usermode software unless they are trying to do 
    // bare-metal things (e.g. early 3DS homebrew writes directly to the FB @ 0x20184E60, etc.
    } else if (((addr & 0xF0000000) == MEM_FCRAM_PADDR) && (addr < (MEM_FCRAM_PADDR_END))) {
        return g_fcram + (addr & MEM_FCRAM_MASK);

    //else if ((addr & 0x3F800000) == 0x04000000) {
    //    return g_vram + (addr & MEM_VRAM_MASK);
    //}
    //else if ((addr & 0x3F000000) >= 0x08000000 && (addr & 0x3F000000) < 0x08000000 + g_MemorySize) {
    //    return m_pRAM + (addr & g_MemoryMask);
    //}
    } else {
        //ERROR_LOG(MEMMAP, "Unknown GetPointer %08x PC %08x LR %08x", addr, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
        ERROR_LOG(MEMMAP, "Unknown GetPointer %08x", addr);
        static bool reported = false;
        //if (!reported) {
        //    Reporting::ReportMessage("Unknown GetPointer %08x PC %08x LR %08x", addr, currentMIPS->pc, currentMIPS->r[MIPS_REG_RA]);
        //    reported = true;
        //}
        //if (!g_Config.bIgnoreBadMemAccess) {
        //    Core_EnableStepping(true);
        //    host->SetDebugMode(true);
        //}
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
