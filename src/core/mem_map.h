// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common.h"
#include "common/common_types.h"

namespace Memory {

////////////////////////////////////////////////////////////////////////////////////////////////////

enum {
    BOOTROM_SIZE            = 0x00010000,   ///< Bootrom (super secret code/data @ 0x8000) size
    MPCORE_PRIV_SIZE        = 0x00002000,   ///< MPCore private memory region size
    VRAM_SIZE               = 0x00600000,   ///< VRAM size
    DSP_SIZE                = 0x00080000,   ///< DSP memory size
    AXI_WRAM_SIZE           = 0x00080000,   ///< AXI WRAM size

    FCRAM_SIZE              = 0x08000000,   ///< FCRAM size
    FCRAM_PADDR             = 0x20000000,                       ///< FCRAM physical address
    FCRAM_PADDR_END         = (FCRAM_PADDR + FCRAM_SIZE),       ///< FCRAM end of physical space
    FCRAM_VADDR             = 0x08000000,                       ///< FCRAM virtual address
    FCRAM_VADDR_END         = (FCRAM_VADDR + FCRAM_SIZE),       ///< FCRAM end of virtual space
    FCRAM_VADDR_FW0B        = 0xF0000000,                       ///< FCRAM adress for firmare FW0B
    FCRAM_VADDR_FW0B_END    = (FCRAM_VADDR_FW0B + FCRAM_SIZE),  ///< FCRAM adress end for FW0B
    FCRAM_MASK              = (FCRAM_SIZE - 1),                 ///< FCRAM mask

    SHARED_MEMORY_SIZE      = 0x04000000,   ///< Shared memory size
    SHARED_MEMORY_VADDR     = 0x10000000,   ///< Shared memory
    SHARED_MEMORY_VADDR_END = (SHARED_MEMORY_VADDR + SHARED_MEMORY_SIZE),
    SHARED_MEMORY_MASK      = (SHARED_MEMORY_SIZE - 1),

    EXEFS_CODE_SIZE         = 0x03F00000,
    EXEFS_CODE_VADDR        = 0x00100000,   ///< ExeFS:/.code is loaded here
    EXEFS_CODE_VADDR_END    = (EXEFS_CODE_VADDR + EXEFS_CODE_SIZE),
    EXEFS_CODE_MASK         = (EXEFS_CODE_VADDR - 1),

    HEAP_SIZE               = FCRAM_SIZE,   ///< Application heap size
    //HEAP_PADDR              = HEAP_GSP_SIZE,
    //HEAP_PADDR_END          = (HEAP_PADDR + HEAP_SIZE),
    HEAP_VADDR              = 0x08000000,
    HEAP_VADDR_END          = (HEAP_VADDR + HEAP_SIZE),
    HEAP_MASK               = (HEAP_SIZE - 1),

    HEAP_GSP_SIZE           = 0x02000000,   ///< GSP heap size... TODO: Define correctly?
    HEAP_GSP_VADDR          = 0x14000000,
    HEAP_GSP_VADDR_END      = (HEAP_GSP_VADDR + HEAP_GSP_SIZE),
    HEAP_GSP_PADDR          = 0x00000000,
    HEAP_GSP_PADDR_END      = (HEAP_GSP_PADDR + HEAP_GSP_SIZE),
    HEAP_GSP_MASK           = (HEAP_GSP_SIZE - 1),

    HARDWARE_IO_SIZE        = 0x01000000,
    HARDWARE_IO_PADDR       = 0x10000000,                       ///< IO physical address start
    HARDWARE_IO_VADDR       = 0x1EC00000,                       ///< IO virtual address start
    HARDWARE_IO_PADDR_END   = (HARDWARE_IO_PADDR + HARDWARE_IO_SIZE),
    HARDWARE_IO_VADDR_END   = (HARDWARE_IO_VADDR + HARDWARE_IO_SIZE),

    VRAM_PADDR              = 0x18000000,
    VRAM_VADDR              = 0x1F000000,
    VRAM_PADDR_END          = (VRAM_PADDR + VRAM_SIZE),
    VRAM_VADDR_END          = (VRAM_VADDR + VRAM_SIZE),
    VRAM_MASK               = 0x007FFFFF,

    SCRATCHPAD_SIZE         = 0x00004000,   ///< Typical stack size - TODO: Read from exheader
    SCRATCHPAD_VADDR_END    = 0x10000000,
    SCRATCHPAD_VADDR        = (SCRATCHPAD_VADDR_END - SCRATCHPAD_SIZE), ///< Stack space
    SCRATCHPAD_MASK         = (SCRATCHPAD_SIZE - 1),            ///< Scratchpad memory mask
};

////////////////////////////////////////////////////////////////////////////////////////////////////

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

////////////////////////////////////////////////////////////////////////////////////////////////////

// Base is a pointer to the base of the memory map. Yes, some MMU tricks
// are used to set up a full GC or Wii memory map in process memory.  on
// 32-bit, you have to mask your offsets with 0x3FFFFFFF. This means that
// some things are mirrored too many times, but eh... it works.

// In 64-bit, this might point to "high memory" (above the 32-bit limit),
// so be sure to load it into a 64-bit register.
extern u8 *g_base; 

// These are guaranteed to point to "low memory" addresses (sub-32-bit).
// 64-bit: Pointers to low-mem (sub-0x10000000) mirror
// 32-bit: Same as the corresponding physical/virtual pointers.
extern u8* g_heap_gsp;      ///< GSP heap (main memory)
extern u8* g_heap;          ///< Application heap (main memory)
extern u8* g_vram;          ///< Video memory (VRAM)
extern u8* g_shared_mem;    ///< Shared memory
extern u8* g_exefs_code;    ///< ExeFS:/.code is loaded here

void Init();
void Shutdown();

u8 Read8(const u32 addr);
u16 Read16(const u32 addr);
u32 Read32(const u32 addr);

u32 Read8_ZX(const u32 addr);
u32 Read16_ZX(const u32 addr);

void Write8(const u32 addr, const u8 data);
void Write16(const u32 addr, const u16 data);
void Write32(const u32 addr, const u32 data);

u8* GetPointer(const u32 Address);

/**
 * Maps a block of memory in shared memory
 * @param handle Handle to map memory block for
 * @param addr Address to map memory block to
 * @param permissions Memory map permissions
 */
u32 MapBlock_Shared(u32 handle, u32 addr,u32 permissions) ;

/**
 * Maps a block of memory on the heap
 * @param size Size of block in bytes
 * @param operation Memory map operation type
 * @param flags Memory allocation flags
 */
u32 MapBlock_Heap(u32 size, u32 operation, u32 permissions);

/**
 * Maps a block of memory on the GSP heap
 * @param size Size of block in bytes
 * @param operation Memory map operation type
 * @param permissions Control memory permissions
 */
u32 MapBlock_HeapGSP(u32 size, u32 operation, u32 permissions);

inline const char* GetCharPointer(const u32 address) {
    return (const char *)GetPointer(address);
}

inline const u32 VirtualAddressFromPhysical_FCRAM(const u32 address) {
    return ((address & FCRAM_MASK) | FCRAM_VADDR);
}

inline const u32 VirtualAddressFromPhysical_IO(const u32 address) {
    return (address + 0x0EB00000);
}

inline const u32 VirtualAddressFromPhysical_VRAM(const u32 address) {
    return (address + 0x07000000);
}

} // namespace
