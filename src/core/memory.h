// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <string>
#include <vector>
#include <boost/optional.hpp>
#include "common/common_types.h"
#include "core/mmio.h"

namespace Kernel {
class Process;
}

namespace Memory {

/**
 * Page size used by the ARM architecture. This is the smallest granularity with which memory can
 * be mapped.
 */
const u32 PAGE_SIZE = 0x1000;
const u32 PAGE_MASK = PAGE_SIZE - 1;
const int PAGE_BITS = 12;
const size_t PAGE_TABLE_NUM_ENTRIES = 1 << (32 - PAGE_BITS);

enum class PageType {
    /// Page is unmapped and should cause an access error.
    Unmapped,
    /// Page is mapped to regular memory. This is the only type you can get pointers to.
    Memory,
    /// Page is mapped to regular memory, but also needs to check for rasterizer cache flushing and
    /// invalidation
    RasterizerCachedMemory,
    /// Page is mapped to a I/O region. Writing and reading to this page is handled by functions.
    Special,
};

struct SpecialRegion {
    VAddr base;
    u32 size;
    MMIORegionPointer handler;
};

/**
 * A (reasonably) fast way of allowing switchable and remappable process address spaces. It loosely
 * mimics the way a real CPU page table works, but instead is optimized for minimal decoding and
 * fetching requirements when accessing. In the usual case of an access to regular memory, it only
 * requires an indexed fetch and a check for NULL.
 */
struct PageTable {
    /**
     * Array of memory pointers backing each page. An entry can only be non-null if the
     * corresponding entry in the `attributes` array is of type `Memory`.
     */
    std::array<u8*, PAGE_TABLE_NUM_ENTRIES> pointers;

    /**
     * Contains MMIO handlers that back memory regions whose entries in the `attribute` array is of
     * type `Special`.
     */
    std::vector<SpecialRegion> special_regions;

    /**
     * Array of fine grained page attributes. If it is set to any value other than `Memory`, then
     * the corresponding entry in `pointers` MUST be set to null.
     */
    std::array<PageType, PAGE_TABLE_NUM_ENTRIES> attributes;
};

/// Physical memory regions as seen from the ARM11
enum : PAddr {
    /// IO register area
    IO_AREA_PADDR = 0x10100000,
    IO_AREA_SIZE = 0x01000000, ///< IO area size (16MB)
    IO_AREA_PADDR_END = IO_AREA_PADDR + IO_AREA_SIZE,

    /// MPCore internal memory region
    MPCORE_RAM_PADDR = 0x17E00000,
    MPCORE_RAM_SIZE = 0x00002000, ///< MPCore internal memory size (8KB)
    MPCORE_RAM_PADDR_END = MPCORE_RAM_PADDR + MPCORE_RAM_SIZE,

    /// Video memory
    VRAM_PADDR = 0x18000000,
    VRAM_SIZE = 0x00600000, ///< VRAM size (6MB)
    VRAM_PADDR_END = VRAM_PADDR + VRAM_SIZE,

    /// New 3DS additional memory. Supposedly faster than regular FCRAM. Part of it can be used by
    /// applications and system modules if mapped via the ExHeader.
    N3DS_EXTRA_RAM_PADDR = 0x1F000000,
    N3DS_EXTRA_RAM_SIZE = 0x00400000, ///< New 3DS additional memory size (4MB)
    N3DS_EXTRA_RAM_PADDR_END = N3DS_EXTRA_RAM_PADDR + N3DS_EXTRA_RAM_SIZE,

    /// DSP memory
    DSP_RAM_PADDR = 0x1FF00000,
    DSP_RAM_SIZE = 0x00080000, ///< DSP memory size (512KB)
    DSP_RAM_PADDR_END = DSP_RAM_PADDR + DSP_RAM_SIZE,

    /// AXI WRAM
    AXI_WRAM_PADDR = 0x1FF80000,
    AXI_WRAM_SIZE = 0x00080000, ///< AXI WRAM size (512KB)
    AXI_WRAM_PADDR_END = AXI_WRAM_PADDR + AXI_WRAM_SIZE,

    /// Main FCRAM
    FCRAM_PADDR = 0x20000000,
    FCRAM_SIZE = 0x08000000,      ///< FCRAM size on the Old 3DS (128MB)
    FCRAM_N3DS_SIZE = 0x10000000, ///< FCRAM size on the New 3DS (256MB)
    FCRAM_PADDR_END = FCRAM_PADDR + FCRAM_SIZE,
    FCRAM_N3DS_PADDR_END = FCRAM_PADDR + FCRAM_N3DS_SIZE,
};

/// Virtual user-space memory regions
enum : VAddr {
    /// Where the application text, data and bss reside.
    PROCESS_IMAGE_VADDR = 0x00100000,
    PROCESS_IMAGE_MAX_SIZE = 0x03F00000,
    PROCESS_IMAGE_VADDR_END = PROCESS_IMAGE_VADDR + PROCESS_IMAGE_MAX_SIZE,

    /// Area where IPC buffers are mapped onto.
    IPC_MAPPING_VADDR = 0x04000000,
    IPC_MAPPING_SIZE = 0x04000000,
    IPC_MAPPING_VADDR_END = IPC_MAPPING_VADDR + IPC_MAPPING_SIZE,

    /// Application heap (includes stack).
    HEAP_VADDR = 0x08000000,
    HEAP_SIZE = 0x08000000,
    HEAP_VADDR_END = HEAP_VADDR + HEAP_SIZE,

    /// Area where shared memory buffers are mapped onto.
    SHARED_MEMORY_VADDR = 0x10000000,
    SHARED_MEMORY_SIZE = 0x04000000,
    SHARED_MEMORY_VADDR_END = SHARED_MEMORY_VADDR + SHARED_MEMORY_SIZE,

    /// Maps 1:1 to an offset in FCRAM. Used for HW allocations that need to be linear in physical
    /// memory.
    LINEAR_HEAP_VADDR = 0x14000000,
    LINEAR_HEAP_SIZE = 0x08000000,
    LINEAR_HEAP_VADDR_END = LINEAR_HEAP_VADDR + LINEAR_HEAP_SIZE,

    /// Maps 1:1 to New 3DS additional memory
    N3DS_EXTRA_RAM_VADDR = 0x1E800000,
    N3DS_EXTRA_RAM_VADDR_END = N3DS_EXTRA_RAM_VADDR + N3DS_EXTRA_RAM_SIZE,

    /// Maps 1:1 to the IO register area.
    IO_AREA_VADDR = 0x1EC00000,
    IO_AREA_VADDR_END = IO_AREA_VADDR + IO_AREA_SIZE,

    /// Maps 1:1 to VRAM.
    VRAM_VADDR = 0x1F000000,
    VRAM_VADDR_END = VRAM_VADDR + VRAM_SIZE,

    /// Maps 1:1 to DSP memory.
    DSP_RAM_VADDR = 0x1FF00000,
    DSP_RAM_VADDR_END = DSP_RAM_VADDR + DSP_RAM_SIZE,

    /// Read-only page containing kernel and system configuration values.
    CONFIG_MEMORY_VADDR = 0x1FF80000,
    CONFIG_MEMORY_SIZE = 0x00001000,
    CONFIG_MEMORY_VADDR_END = CONFIG_MEMORY_VADDR + CONFIG_MEMORY_SIZE,

    /// Usually read-only page containing mostly values read from hardware.
    SHARED_PAGE_VADDR = 0x1FF81000,
    SHARED_PAGE_SIZE = 0x00001000,
    SHARED_PAGE_VADDR_END = SHARED_PAGE_VADDR + SHARED_PAGE_SIZE,

    /// Area where TLS (Thread-Local Storage) buffers are allocated.
    TLS_AREA_VADDR = 0x1FF82000,
    TLS_ENTRY_SIZE = 0x200,

    /// Equivalent to LINEAR_HEAP_VADDR, but expanded to cover the extra memory in the New 3DS.
    NEW_LINEAR_HEAP_VADDR = 0x30000000,
    NEW_LINEAR_HEAP_SIZE = 0x10000000,
    NEW_LINEAR_HEAP_VADDR_END = NEW_LINEAR_HEAP_VADDR + NEW_LINEAR_HEAP_SIZE,
};

/// Currently active page table
void SetCurrentPageTable(PageTable* page_table);
PageTable* GetCurrentPageTable();

/// Determines if the given VAddr is valid for the specified process.
bool IsValidVirtualAddress(const Kernel::Process& process, VAddr vaddr);
bool IsValidVirtualAddress(VAddr vaddr);

bool IsValidPhysicalAddress(PAddr paddr);

u8 Read8(VAddr addr);
u16 Read16(VAddr addr);
u32 Read32(VAddr addr);
u64 Read64(VAddr addr);

void Write8(VAddr addr, u8 data);
void Write16(VAddr addr, u16 data);
void Write32(VAddr addr, u32 data);
void Write64(VAddr addr, u64 data);

void ReadBlock(const Kernel::Process& process, VAddr src_addr, void* dest_buffer, size_t size);
void ReadBlock(VAddr src_addr, void* dest_buffer, size_t size);
void WriteBlock(const Kernel::Process& process, VAddr dest_addr, const void* src_buffer,
                size_t size);
void WriteBlock(VAddr dest_addr, const void* src_buffer, size_t size);
void ZeroBlock(const Kernel::Process& process, VAddr dest_addr, const size_t size);
void ZeroBlock(VAddr dest_addr, const size_t size);
void CopyBlock(const Kernel::Process& process, VAddr dest_addr, VAddr src_addr, size_t size);
void CopyBlock(VAddr dest_addr, VAddr src_addr, size_t size);

u8* GetPointer(VAddr vaddr);

std::string ReadCString(VAddr vaddr, std::size_t max_length);

/**
 * Converts a virtual address inside a region with 1:1 mapping to physical memory to a physical
 * address. This should be used by services to translate addresses for use by the hardware.
 */
boost::optional<PAddr> TryVirtualToPhysicalAddress(VAddr addr);

/**
 * Converts a virtual address inside a region with 1:1 mapping to physical memory to a physical
 * address. This should be used by services to translate addresses for use by the hardware.
 *
 * @deprecated Use TryVirtualToPhysicalAddress(), which reports failure.
 */
PAddr VirtualToPhysicalAddress(VAddr addr);

/**
 * Undoes a mapping performed by VirtualToPhysicalAddress().
 */
boost::optional<VAddr> PhysicalToVirtualAddress(PAddr paddr);

/**
 * Gets a pointer to the memory region beginning at the specified physical address.
 */
u8* GetPhysicalPointer(PAddr address);

/**
 * Mark each page touching the region as cached.
 */
void RasterizerMarkRegionCached(PAddr start, u32 size, bool cached);

/**
 * Flushes any externally cached rasterizer resources touching the given region.
 */
void RasterizerFlushRegion(PAddr start, u32 size);

/**
 * Invalidates any externally cached rasterizer resources touching the given region.
 */
void RasterizerInvalidateRegion(PAddr start, u32 size);

/**
 * Flushes and invalidates any externally cached rasterizer resources touching the given region.
 */
void RasterizerFlushAndInvalidateRegion(PAddr start, u32 size);

enum class FlushMode {
    /// Write back modified surfaces to RAM
    Flush,
    /// Remove region from the cache
    Invalidate,
    /// Write back modified surfaces to RAM, and also remove them from the cache
    FlushAndInvalidate,
};

/**
 * Flushes and invalidates any externally cached rasterizer resources touching the given virtual
 * address region.
 */
void RasterizerFlushVirtualRegion(VAddr start, u32 size, FlushMode mode);

} // namespace Memory
