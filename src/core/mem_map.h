// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Memory {

////////////////////////////////////////////////////////////////////////////////////////////////////

const u32 PAGE_SIZE = 0x1000;

/// Physical memory regions as seen from the ARM11
enum : PAddr {
    /// IO register area
    IO_AREA_PADDR     = 0x10100000,
    IO_AREA_SIZE      = 0x01000000, ///< IO area size (16MB)
    IO_AREA_PADDR_END = IO_AREA_PADDR + IO_AREA_SIZE,

    /// MPCore internal memory region
    MPCORE_RAM_PADDR     = 0x17E00000,
    MPCORE_RAM_SIZE      = 0x00002000, ///< MPCore internal memory size (8KB)
    MPCORE_RAM_PADDR_END = MPCORE_RAM_PADDR + MPCORE_RAM_SIZE,

    /// Video memory
    VRAM_PADDR     = 0x18000000,
    VRAM_SIZE      = 0x00600000, ///< VRAM size (6MB)
    VRAM_PADDR_END = VRAM_PADDR + VRAM_SIZE,

    /// DSP memory
    DSP_RAM_PADDR     = 0x1FF00000,
    DSP_RAM_SIZE      = 0x00080000, ///< DSP memory size (512KB)
    DSP_RAM_PADDR_END = DSP_RAM_PADDR + DSP_RAM_SIZE,

    /// AXI WRAM
    AXI_WRAM_PADDR     = 0x1FF80000,
    AXI_WRAM_SIZE      = 0x00080000, ///< AXI WRAM size (512KB)
    AXI_WRAM_PADDR_END = AXI_WRAM_PADDR + AXI_WRAM_SIZE,

    /// Main FCRAM
    FCRAM_PADDR     = 0x20000000,
    FCRAM_SIZE      = 0x08000000, ///< FCRAM size (128MB)
    FCRAM_PADDR_END = FCRAM_PADDR + FCRAM_SIZE,
};

/// Virtual user-space memory regions
enum : VAddr {
    /// Where the application text, data and bss reside.
    PROCESS_IMAGE_VADDR     = 0x00100000,
    PROCESS_IMAGE_MAX_SIZE  = 0x03F00000,
    PROCESS_IMAGE_VADDR_END = PROCESS_IMAGE_VADDR + PROCESS_IMAGE_MAX_SIZE,

    /// Area where IPC buffers are mapped onto.
    IPC_MAPPING_VADDR     = 0x04000000,
    IPC_MAPPING_SIZE      = 0x04000000,
    IPC_MAPPING_VADDR_END = IPC_MAPPING_VADDR + IPC_MAPPING_SIZE,

    /// Application heap (includes stack).
    HEAP_VADDR     = 0x08000000,
    HEAP_SIZE      = 0x08000000,
    HEAP_VADDR_END = HEAP_VADDR + HEAP_SIZE,

    /// Area where shared memory buffers are mapped onto.
    SHARED_MEMORY_VADDR     = 0x10000000,
    SHARED_MEMORY_SIZE      = 0x04000000,
    SHARED_MEMORY_VADDR_END = SHARED_MEMORY_VADDR + SHARED_MEMORY_SIZE,

    /// Maps 1:1 to an offset in FCRAM. Used for HW allocations that need to be linear in physical memory.
    LINEAR_HEAP_VADDR     = 0x14000000,
    LINEAR_HEAP_SIZE      = 0x08000000,
    LINEAR_HEAP_VADDR_END = LINEAR_HEAP_VADDR + LINEAR_HEAP_SIZE,

    /// Maps 1:1 to the IO register area.
    IO_AREA_VADDR     = 0x1EC00000,
    IO_AREA_VADDR_END = IO_AREA_VADDR + IO_AREA_SIZE,

    /// Maps 1:1 to VRAM.
    VRAM_VADDR     = 0x1F000000,
    VRAM_VADDR_END = VRAM_VADDR + VRAM_SIZE,

    /// Maps 1:1 to DSP memory.
    DSP_RAM_VADDR     = 0x1FF00000,
    DSP_RAM_VADDR_END = DSP_RAM_VADDR + DSP_RAM_SIZE,

    /// Read-only page containing kernel and system configuration values.
    CONFIG_MEMORY_VADDR     = 0x1FF80000,
    CONFIG_MEMORY_SIZE      = 0x00001000,
    CONFIG_MEMORY_VADDR_END = CONFIG_MEMORY_VADDR + CONFIG_MEMORY_SIZE,

    /// Usually read-only page containing mostly values read from hardware.
    SHARED_PAGE_VADDR     = 0x1FF81000,
    SHARED_PAGE_SIZE      = 0x00001000,
    SHARED_PAGE_VADDR_END = SHARED_PAGE_VADDR + SHARED_PAGE_SIZE,

    // TODO(yuriks): The exact location and size of this area is uncomfirmed.
    /// Area where TLS (Thread-Local Storage) buffers are allocated.
    TLS_AREA_VADDR     = 0x1FFA0000,
    TLS_AREA_SIZE      = 0x00002000, // Each TLS buffer is 0x200 bytes, allows for 16 threads
    TLS_AREA_VADDR_END = TLS_AREA_VADDR + TLS_AREA_SIZE,
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

extern u8* g_exefs_code;  ///< ExeFS:/.code is loaded here
extern u8* g_heap;        ///< Application heap (main memory)
extern u8* g_shared_mem;  ///< Shared memory
extern u8* g_heap_linear; ///< Linear heap (main memory)
extern u8* g_vram;        ///< Video memory (VRAM)
extern u8* g_dsp_mem;     ///< DSP memory
extern u8* g_tls_mem;     ///< TLS memory

void Init();
void Shutdown();

template <typename T>
inline void Read(T &var, VAddr addr);

template <typename T>
inline void Write(VAddr addr, T data);

u8 Read8(VAddr addr);
u16 Read16(VAddr addr);
u32 Read32(VAddr addr);
u64 Read64(VAddr addr);

u32 Read8_ZX(VAddr addr);
u32 Read16_ZX(VAddr addr);

void Write8(VAddr addr, u8 data);
void Write16(VAddr addr, u16 data);
void Write32(VAddr addr, u32 data);
void Write64(VAddr addr, u64 data);

void WriteBlock(VAddr addr, const u8* data, size_t size);

u8* GetPointer(VAddr virtual_address);

/**
 * Maps a block of memory on the heap
 * @param size Size of block in bytes
 * @param operation Memory map operation type
 * @param permissions Memory allocation permissions
 */
u32 MapBlock_Heap(u32 size, u32 operation, u32 permissions);

/**
 * Maps a block of memory on the GSP heap
 * @param size Size of block in bytes
 * @param operation Memory map operation type
 * @param permissions Control memory permissions
 */
u32 MapBlock_HeapLinear(u32 size, u32 operation, u32 permissions);

/// Initialize mapped memory blocks
void MemBlock_Init();

/// Shutdown mapped memory blocks
void MemBlock_Shutdown();

inline const char* GetCharPointer(const VAddr address) {
    return (const char *)GetPointer(address);
}

/**
 * Converts a virtual address inside a region with 1:1 mapping to physical memory to a physical
 * address. This should be used by services to translate addresses for use by the hardware.
 */
PAddr VirtualToPhysicalAddress(VAddr addr);

/**
 * Undoes a mapping performed by VirtualToPhysicalAddress().
 */
VAddr PhysicalToVirtualAddress(PAddr addr);

/**
 * Gets a pointer to the memory region beginning at the specified physical address.
 *
 * @note This is currently implemented using PhysicalToVirtualAddress().
 */
inline u8* GetPhysicalPointer(PAddr address) {
    return GetPointer(PhysicalToVirtualAddress(address));
}

} // namespace
