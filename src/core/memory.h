// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include <boost/serialization/array.hpp>
#include <boost/serialization/vector.hpp>
#include "common/common_types.h"
#include "common/memory_ref.h"
#include "core/mmio.h"

class ARM_Interface;

namespace Kernel {
class Process;
}

namespace AudioCore {
class DspInterface;
}

namespace Memory {

// Are defined in a system header
#undef PAGE_SIZE
#undef PAGE_MASK
/**
 * Page size used by the ARM architecture. This is the smallest granularity with which memory can
 * be mapped.
 */
const u32 PAGE_SIZE = 0x1000;
const u32 PAGE_MASK = PAGE_SIZE - 1;
const int PAGE_BITS = 12;
const std::size_t PAGE_TABLE_NUM_ENTRIES = 1 << (32 - PAGE_BITS);

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

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version) {
        ar& base;
        ar& size;
        ar& handler;
    }
    friend class boost::serialization::access;
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

    // The reason for this rigmarole is to keep the 'raw' and 'refs' arrays in sync.
    // We need 'raw' for dynarmic and 'refs' for serialization
    struct Pointers {

        struct Entry {
            Entry(Pointers& pointers_, VAddr idx_) : pointers(pointers_), idx(idx_) {}

            void operator=(MemoryRef value) {
                pointers.refs[idx] = value;
                pointers.raw[idx] = value.GetPtr();
            }

            operator u8*() {
                return pointers.raw[idx];
            }

        private:
            Pointers& pointers;
            VAddr idx;
        };

        Entry operator[](VAddr idx) {
            return Entry(*this, idx);
        }

        u8* operator[](VAddr idx) const {
            return raw[idx];
        }

        Entry operator[](std::size_t idx) {
            return Entry(*this, static_cast<VAddr>(idx));
        }

    private:
        std::array<u8*, PAGE_TABLE_NUM_ENTRIES> raw;

        std::array<MemoryRef, PAGE_TABLE_NUM_ENTRIES> refs;

        friend struct PageTable;
    };
    Pointers pointers;

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

    std::array<u8*, PAGE_TABLE_NUM_ENTRIES>& GetPointerArray() {
        return pointers.raw;
    }

    void Clear();

private:
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& pointers.refs;
        ar& special_regions;
        ar& attributes;
        for (auto i = 0; i < PAGE_TABLE_NUM_ENTRIES; i++) {
            pointers.raw[i] = pointers.refs[i].GetPtr();
        }
    }
    friend class boost::serialization::access;
};

/// Physical memory regions as seen from the ARM11
enum : PAddr {
    /// IO register area
    IO_AREA_PADDR = 0x10100000,
    IO_AREA_SIZE = 0x00400000, ///< IO area size (4MB)
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

enum class Region { FCRAM, VRAM, DSP, N3DS };

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
 * Flushes and invalidates all memory in the rasterizer cache and removes any leftover state
 * If flush is true, the rasterizer should flush any cached resources to RAM before clearing
 */
void RasterizerClearAll(bool flush);

/**
 * Flushes and invalidates any externally cached rasterizer resources touching the given virtual
 * address region.
 */
void RasterizerFlushVirtualRegion(VAddr start, u32 size, FlushMode mode);

class MemorySystem {
public:
    MemorySystem();
    ~MemorySystem();

    /**
     * Maps an allocated buffer onto a region of the emulated process address space.
     *
     * @param page_table The page table of the emulated process.
     * @param base The address to start mapping at. Must be page-aligned.
     * @param size The amount of bytes to map. Must be page-aligned.
     * @param target Buffer with the memory backing the mapping. Must be of length at least `size`.
     */
    void MapMemoryRegion(PageTable& page_table, VAddr base, u32 size, MemoryRef target);

    /**
     * Maps a region of the emulated process address space as a IO region.
     * @param page_table The page table of the emulated process.
     * @param base The address to start mapping at. Must be page-aligned.
     * @param size The amount of bytes to map. Must be page-aligned.
     * @param mmio_handler The handler that backs the mapping.
     */
    void MapIoRegion(PageTable& page_table, VAddr base, u32 size, MMIORegionPointer mmio_handler);

    void UnmapRegion(PageTable& page_table, VAddr base, u32 size);

    /// Currently active page table
    void SetCurrentPageTable(std::shared_ptr<PageTable> page_table);
    std::shared_ptr<PageTable> GetCurrentPageTable() const;

    u8 Read8(VAddr addr);
    u16 Read16(VAddr addr);
    u32 Read32(VAddr addr);
    u64 Read64(VAddr addr);

    void Write8(VAddr addr, u8 data);
    void Write16(VAddr addr, u16 data);
    void Write32(VAddr addr, u32 data);
    void Write64(VAddr addr, u64 data);

    void ReadBlock(const Kernel::Process& process, VAddr src_addr, void* dest_buffer,
                   std::size_t size);
    void WriteBlock(const Kernel::Process& process, VAddr dest_addr, const void* src_buffer,
                    std::size_t size);
    void ZeroBlock(const Kernel::Process& process, VAddr dest_addr, const std::size_t size);
    void CopyBlock(const Kernel::Process& process, VAddr dest_addr, VAddr src_addr,
                   std::size_t size);
    void CopyBlock(const Kernel::Process& dest_process, const Kernel::Process& src_process,
                   VAddr dest_addr, VAddr src_addr, std::size_t size);

    std::string ReadCString(VAddr vaddr, std::size_t max_length);

    /**
     * Gets a pointer to the memory region beginning at the specified physical address.
     */
    u8* GetPhysicalPointer(PAddr address);

    MemoryRef GetPhysicalRef(PAddr address);

    u8* GetPointer(VAddr vaddr);

    bool IsValidPhysicalAddress(PAddr paddr);

    /// Gets offset in FCRAM from a pointer inside FCRAM range
    u32 GetFCRAMOffset(const u8* pointer);

    /// Gets pointer in FCRAM with given offset
    u8* GetFCRAMPointer(u32 offset);

    /// Gets a serializable ref to FCRAM with the given offset
    MemoryRef GetFCRAMRef(u32 offset);

    /**
     * Mark each page touching the region as cached.
     */
    void RasterizerMarkRegionCached(PAddr start, u32 size, bool cached);

    /// Registers page table for rasterizer cache marking
    void RegisterPageTable(std::shared_ptr<PageTable> page_table);

    /// Unregisters page table for rasterizer cache marking
    void UnregisterPageTable(std::shared_ptr<PageTable> page_table);

    void SetDSP(AudioCore::DspInterface& dsp);

private:
    template <typename T>
    T Read(const VAddr vaddr);

    template <typename T>
    void Write(const VAddr vaddr, const T data);

    /**
     * Gets the pointer for virtual memory where the page is marked as RasterizerCachedMemory.
     * This is used to access the memory where the page pointer is nullptr due to rasterizer cache.
     * Since the cache only happens on linear heap or VRAM, we know the exact physical address and
     * pointer of such virtual address
     */
    MemoryRef GetPointerForRasterizerCache(VAddr addr);

    void MapPages(PageTable& page_table, u32 base, u32 size, MemoryRef memory, PageType type);

    class Impl;

    std::unique_ptr<Impl> impl;

    friend class boost::serialization::access;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int file_version);

public:
    template <Region R>
    class BackingMemImpl;
};

/// Determines if the given VAddr is valid for the specified process.
bool IsValidVirtualAddress(const Kernel::Process& process, VAddr vaddr);

} // namespace Memory

BOOST_CLASS_EXPORT_KEY(Memory::MemorySystem::BackingMemImpl<Memory::Region::FCRAM>)
BOOST_CLASS_EXPORT_KEY(Memory::MemorySystem::BackingMemImpl<Memory::Region::VRAM>)
BOOST_CLASS_EXPORT_KEY(Memory::MemorySystem::BackingMemImpl<Memory::Region::DSP>)
BOOST_CLASS_EXPORT_KEY(Memory::MemorySystem::BackingMemImpl<Memory::Region::N3DS>)
