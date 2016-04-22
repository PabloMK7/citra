// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstring>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/swap.h"

#include "core/hle/kernel/process.h"
#include "core/memory.h"
#include "core/memory_setup.h"
#include "core/mmio.h"

#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Memory {

enum class PageType {
    /// Page is unmapped and should cause an access error.
    Unmapped,
    /// Page is mapped to regular memory. This is the only type you can get pointers to.
    Memory,
    /// Page is mapped to regular memory, but also needs to check for rasterizer cache flushing and invalidation
    RasterizerCachedMemory,
    /// Page is mapped to a I/O region. Writing and reading to this page is handled by functions.
    Special,
    /// Page is mapped to a I/O region, but also needs to check for rasterizer cache flushing and invalidation
    RasterizerCachedSpecial,
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
    static const size_t NUM_ENTRIES = 1 << (32 - PAGE_BITS);

    /**
     * Array of memory pointers backing each page. An entry can only be non-null if the
     * corresponding entry in the `attributes` array is of type `Memory`.
     */
    std::array<u8*, NUM_ENTRIES> pointers;

    /**
     * Contains MMIO handlers that back memory regions whose entries in the `attribute` array is of type `Special`.
     */
    std::vector<SpecialRegion> special_regions;

    /**
     * Array of fine grained page attributes. If it is set to any value other than `Memory`, then
     * the corresponding entry in `pointers` MUST be set to null.
     */
    std::array<PageType, NUM_ENTRIES> attributes;

    /**
     * Indicates the number of externally cached resources touching a page that should be
     * flushed before the memory is accessed
     */
    std::array<u8, NUM_ENTRIES> cached_res_count;
};

/// Singular page table used for the singleton process
static PageTable main_page_table;
/// Currently active page table
static PageTable* current_page_table = &main_page_table;

static void MapPages(u32 base, u32 size, u8* memory, PageType type) {
    LOG_DEBUG(HW_Memory, "Mapping %p onto %08X-%08X", memory, base * PAGE_SIZE, (base + size) * PAGE_SIZE);

    u32 end = base + size;

    while (base != end) {
        ASSERT_MSG(base < PageTable::NUM_ENTRIES, "out of range mapping at %08X", base);

        // Since pages are unmapped on shutdown after video core is shutdown, the renderer may be null here
        if (current_page_table->attributes[base] == PageType::RasterizerCachedMemory ||
            current_page_table->attributes[base] == PageType::RasterizerCachedSpecial) {
            RasterizerFlushAndInvalidateRegion(VirtualToPhysicalAddress(base << PAGE_BITS), PAGE_SIZE);
        }

        current_page_table->attributes[base] = type;
        current_page_table->pointers[base] = memory;
        current_page_table->cached_res_count[base] = 0;

        base += 1;
        if (memory != nullptr)
            memory += PAGE_SIZE;
    }
}

void InitMemoryMap() {
    main_page_table.pointers.fill(nullptr);
    main_page_table.attributes.fill(PageType::Unmapped);
    main_page_table.cached_res_count.fill(0);
}

void MapMemoryRegion(VAddr base, u32 size, u8* target) {
    ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: %08X", size);
    ASSERT_MSG((base & PAGE_MASK) == 0, "non-page aligned base: %08X", base);
    MapPages(base / PAGE_SIZE, size / PAGE_SIZE, target, PageType::Memory);
}

void MapIoRegion(VAddr base, u32 size, MMIORegionPointer mmio_handler) {
    ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: %08X", size);
    ASSERT_MSG((base & PAGE_MASK) == 0, "non-page aligned base: %08X", base);
    MapPages(base / PAGE_SIZE, size / PAGE_SIZE, nullptr, PageType::Special);

    current_page_table->special_regions.emplace_back(SpecialRegion{base, size, mmio_handler});
}

void UnmapRegion(VAddr base, u32 size) {
    ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: %08X", size);
    ASSERT_MSG((base & PAGE_MASK) == 0, "non-page aligned base: %08X", base);
    MapPages(base / PAGE_SIZE, size / PAGE_SIZE, nullptr, PageType::Unmapped);
}

/**
 * Gets a pointer to the exact memory at the virtual address (i.e. not page aligned)
 * using a VMA from the current process
 */
static u8* GetPointerFromVMA(VAddr vaddr) {
    u8* direct_pointer = nullptr;

    auto& vma = Kernel::g_current_process->vm_manager.FindVMA(vaddr)->second;
    switch (vma.type) {
    case Kernel::VMAType::AllocatedMemoryBlock:
        direct_pointer = vma.backing_block->data() + vma.offset;
        break;
    case Kernel::VMAType::BackingMemory:
        direct_pointer = vma.backing_memory;
        break;
    default:
        UNREACHABLE();
    }

    return direct_pointer + (vaddr - vma.base);
}

/**
 * This function should only be called for virtual addreses with attribute `PageType::Special`.
 */
static MMIORegionPointer GetMMIOHandler(VAddr vaddr) {
    for (const auto& region : current_page_table->special_regions) {
        if (vaddr >= region.base && vaddr < (region.base + region.size)) {
            return region.handler;
        }
    }
    ASSERT_MSG(false, "Mapped IO page without a handler @ %08X", vaddr);
    return nullptr; // Should never happen
}

template<typename T>
T ReadMMIO(MMIORegionPointer mmio_handler, VAddr addr);

template <typename T>
T Read(const VAddr vaddr) {
    const u8* page_pointer = current_page_table->pointers[vaddr >> PAGE_BITS];
    if (page_pointer) {
        // NOTE: Avoid adding any extra logic to this fast-path block
        T value;
        std::memcpy(&value, &page_pointer[vaddr & PAGE_MASK], sizeof(T));
        return value;
    }

    PageType type = current_page_table->attributes[vaddr >> PAGE_BITS];
    switch (type) {
    case PageType::Unmapped:
        LOG_ERROR(HW_Memory, "unmapped Read%lu @ 0x%08X", sizeof(T) * 8, vaddr);
        return 0;
    case PageType::Memory:
        ASSERT_MSG(false, "Mapped memory page without a pointer @ %08X", vaddr);
        break;
    case PageType::RasterizerCachedMemory:
    {
        RasterizerFlushRegion(VirtualToPhysicalAddress(vaddr), sizeof(T));

        T value;
        std::memcpy(&value, GetPointerFromVMA(vaddr), sizeof(T));
        return value;
    }
    case PageType::Special:
        return ReadMMIO<T>(GetMMIOHandler(vaddr), vaddr);
    case PageType::RasterizerCachedSpecial:
    {
        RasterizerFlushRegion(VirtualToPhysicalAddress(vaddr), sizeof(T));

        return ReadMMIO<T>(GetMMIOHandler(vaddr), vaddr);
    }
    default:
        UNREACHABLE();
    }
}

template<typename T>
void WriteMMIO(MMIORegionPointer mmio_handler, VAddr addr, const T data);

template <typename T>
void Write(const VAddr vaddr, const T data) {
    u8* page_pointer = current_page_table->pointers[vaddr >> PAGE_BITS];
    if (page_pointer) {
        // NOTE: Avoid adding any extra logic to this fast-path block
        std::memcpy(&page_pointer[vaddr & PAGE_MASK], &data, sizeof(T));
        return;
    }

    PageType type = current_page_table->attributes[vaddr >> PAGE_BITS];
    switch (type) {
    case PageType::Unmapped:
        LOG_ERROR(HW_Memory, "unmapped Write%lu 0x%08X @ 0x%08X", sizeof(data) * 8, (u32) data, vaddr);
        return;
    case PageType::Memory:
        ASSERT_MSG(false, "Mapped memory page without a pointer @ %08X", vaddr);
        break;
    case PageType::RasterizerCachedMemory:
    {
        RasterizerFlushAndInvalidateRegion(VirtualToPhysicalAddress(vaddr), sizeof(T));

        std::memcpy(GetPointerFromVMA(vaddr), &data, sizeof(T));
        break;
    }
    case PageType::Special:
        WriteMMIO<T>(GetMMIOHandler(vaddr), vaddr, data);
        break;
    case PageType::RasterizerCachedSpecial:
    {
        RasterizerFlushAndInvalidateRegion(VirtualToPhysicalAddress(vaddr), sizeof(T));

        WriteMMIO<T>(GetMMIOHandler(vaddr), vaddr, data);
        break;
    }
    default:
        UNREACHABLE();
    }
}

u8* GetPointer(const VAddr vaddr) {
    u8* page_pointer = current_page_table->pointers[vaddr >> PAGE_BITS];
    if (page_pointer) {
        return page_pointer + (vaddr & PAGE_MASK);
    }

    if (current_page_table->attributes[vaddr >> PAGE_BITS] == PageType::RasterizerCachedMemory) {
        return GetPointerFromVMA(vaddr);
    }

    LOG_ERROR(HW_Memory, "unknown GetPointer @ 0x%08x", vaddr);
    return nullptr;
}

u8* GetPhysicalPointer(PAddr address) {
    return GetPointer(PhysicalToVirtualAddress(address));
}

void RasterizerMarkRegionCached(PAddr start, u32 size, int count_delta) {
    if (start == 0) {
        return;
    }

    u32 num_pages = ((start + size - 1) >> PAGE_BITS) - (start >> PAGE_BITS) + 1;
    PAddr paddr = start;

    for (unsigned i = 0; i < num_pages; ++i) {
        VAddr vaddr = PhysicalToVirtualAddress(paddr);
        u8& res_count = current_page_table->cached_res_count[vaddr >> PAGE_BITS];
        ASSERT_MSG(count_delta <= UINT8_MAX - res_count, "Rasterizer resource cache counter overflow!");
        ASSERT_MSG(count_delta >= -res_count, "Rasterizer resource cache counter underflow!");

        // Switch page type to cached if now cached
        if (res_count == 0) {
            PageType& page_type = current_page_table->attributes[vaddr >> PAGE_BITS];
            switch (page_type) {
            case PageType::Memory:
                page_type = PageType::RasterizerCachedMemory;
                current_page_table->pointers[vaddr >> PAGE_BITS] = nullptr;
                break;
            case PageType::Special:
                page_type = PageType::RasterizerCachedSpecial;
                break;
            default:
                UNREACHABLE();
            }
        }

        res_count += count_delta;

        // Switch page type to uncached if now uncached
        if (res_count == 0) {
            PageType& page_type = current_page_table->attributes[vaddr >> PAGE_BITS];
            switch (page_type) {
            case PageType::RasterizerCachedMemory:
                page_type = PageType::Memory;
                current_page_table->pointers[vaddr >> PAGE_BITS] = GetPointerFromVMA(vaddr & ~PAGE_MASK);
                break;
            case PageType::RasterizerCachedSpecial:
                page_type = PageType::Special;
                break;
            default:
                UNREACHABLE();
            }
        }
        paddr += PAGE_SIZE;
    }
}

void RasterizerFlushRegion(PAddr start, u32 size) {
    if (VideoCore::g_renderer != nullptr) {
        VideoCore::g_renderer->Rasterizer()->FlushRegion(start, size);
    }
}

void RasterizerFlushAndInvalidateRegion(PAddr start, u32 size) {
    if (VideoCore::g_renderer != nullptr) {
        VideoCore::g_renderer->Rasterizer()->FlushAndInvalidateRegion(start, size);
    }
}

u8 Read8(const VAddr addr) {
    return Read<u8>(addr);
}

u16 Read16(const VAddr addr) {
    return Read<u16_le>(addr);
}

u32 Read32(const VAddr addr) {
    return Read<u32_le>(addr);
}

u64 Read64(const VAddr addr) {
    return Read<u64_le>(addr);
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
    for (u32 offset = 0; offset < size; offset++) {
        Write8(addr + offset, data[offset]);
    }
}

template<>
u8 ReadMMIO<u8>(MMIORegionPointer mmio_handler, VAddr addr) {
    return mmio_handler->Read8(addr);
}

template<>
u16 ReadMMIO<u16>(MMIORegionPointer mmio_handler, VAddr addr) {
    return mmio_handler->Read16(addr);
}

template<>
u32 ReadMMIO<u32>(MMIORegionPointer mmio_handler, VAddr addr) {
    return mmio_handler->Read32(addr);
}

template<>
u64 ReadMMIO<u64>(MMIORegionPointer mmio_handler, VAddr addr) {
    return mmio_handler->Read64(addr);
}

template<>
void WriteMMIO<u8>(MMIORegionPointer mmio_handler, VAddr addr, const u8 data) {
    mmio_handler->Write8(addr, data);
}

template<>
void WriteMMIO<u16>(MMIORegionPointer mmio_handler, VAddr addr, const u16 data) {
    mmio_handler->Write16(addr, data);
}

template<>
void WriteMMIO<u32>(MMIORegionPointer mmio_handler, VAddr addr, const u32 data) {
    mmio_handler->Write32(addr, data);
}

template<>
void WriteMMIO<u64>(MMIORegionPointer mmio_handler, VAddr addr, const u64 data) {
    mmio_handler->Write64(addr, data);
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
    } else if (addr >= NEW_LINEAR_HEAP_VADDR && addr < NEW_LINEAR_HEAP_VADDR_END) {
        return addr - NEW_LINEAR_HEAP_VADDR + FCRAM_PADDR;
    }

    LOG_ERROR(HW_Memory, "Unknown virtual address @ 0x%08X", addr);
    // To help with debugging, set bit on address so that it's obviously invalid.
    return addr | 0x80000000;
}

VAddr PhysicalToVirtualAddress(const PAddr addr) {
    if (addr == 0) {
        return 0;
    } else if (addr >= VRAM_PADDR && addr < VRAM_PADDR_END) {
        return addr - VRAM_PADDR + VRAM_VADDR;
    } else if (addr >= FCRAM_PADDR && addr < FCRAM_PADDR_END) {
        return addr - FCRAM_PADDR + Kernel::g_current_process->GetLinearHeapAreaAddress();
    } else if (addr >= DSP_RAM_PADDR && addr < DSP_RAM_PADDR_END) {
        return addr - DSP_RAM_PADDR + DSP_RAM_VADDR;
    } else if (addr >= IO_AREA_PADDR && addr < IO_AREA_PADDR_END) {
        return addr - IO_AREA_PADDR + IO_AREA_VADDR;
    }

    LOG_ERROR(HW_Memory, "Unknown physical address @ 0x%08X", addr);
    // To help with debugging, set bit on address so that it's obviously invalid.
    return addr | 0x80000000;
}

} // namespace
