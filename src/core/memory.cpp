// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>

#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/swap.h"

#include "core/mem_map.h"
#include "core/memory.h"
#include "core/memory_setup.h"

namespace Memory {

enum class PageType {
    /// Page is unmapped and should cause an access error.
    Unmapped,
    /// Page is mapped to regular memory. This is the only type you can get pointers to.
    Memory,
    /// Page is mapped to a I/O region. Writing and reading to this page is handled by functions.
    Special,
};

/**
 * A (reasonably) fast way of allowing switchable and remmapable process address spaces. It loosely
 * mimics the way a real CPU page table works, but instead is optimized for minimal decoding and
 * fetching requirements when acessing. In the usual case of an access to regular memory, it only
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
     * Array of fine grained page attributes. If it is set to any value other than `Memory`, then
     * the corresponding entry in `pointer` MUST be set to null.
     */
    std::array<PageType, NUM_ENTRIES> attributes;
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

        current_page_table->attributes[base] = type;
        current_page_table->pointers[base] = memory;

        base += 1;
        if (memory != nullptr)
            memory += PAGE_SIZE;
    }
}

void InitMemoryMap() {
    main_page_table.pointers.fill(nullptr);
    main_page_table.attributes.fill(PageType::Unmapped);
}

void MapMemoryRegion(VAddr base, u32 size, u8* target) {
    ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: %08X", size);
    ASSERT_MSG((base & PAGE_MASK) == 0, "non-page aligned base: %08X", base);
    MapPages(base / PAGE_SIZE, size / PAGE_SIZE, target, PageType::Memory);
}

void MapIoRegion(VAddr base, u32 size) {
    ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: %08X", size);
    ASSERT_MSG((base & PAGE_MASK) == 0, "non-page aligned base: %08X", base);
    MapPages(base / PAGE_SIZE, size / PAGE_SIZE, nullptr, PageType::Special);
}

void UnmapRegion(VAddr base, u32 size) {
    ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: %08X", size);
    ASSERT_MSG((base & PAGE_MASK) == 0, "non-page aligned base: %08X", base);
    MapPages(base / PAGE_SIZE, size / PAGE_SIZE, nullptr, PageType::Unmapped);
}

template <typename T>
T Read(const VAddr vaddr) {
    const u8* page_pointer = current_page_table->pointers[vaddr >> PAGE_BITS];
    if (page_pointer) {
        return *reinterpret_cast<const T*>(page_pointer + (vaddr & PAGE_MASK));
    }

    PageType type = current_page_table->attributes[vaddr >> PAGE_BITS];
    switch (type) {
    case PageType::Unmapped:
        LOG_ERROR(HW_Memory, "unmapped Read%lu @ 0x%08X", sizeof(T) * 8, vaddr);
        return 0;
    case PageType::Memory:
        ASSERT_MSG(false, "Mapped memory page without a pointer @ %08X", vaddr);
    case PageType::Special:
        LOG_ERROR(HW_Memory, "I/O reads aren't implemented yet @ %08X", vaddr);
        return 0;
    default:
        UNREACHABLE();
    }
}

template <typename T>
void Write(const VAddr vaddr, const T data) {
    u8* page_pointer = current_page_table->pointers[vaddr >> PAGE_BITS];
    if (page_pointer) {
        *reinterpret_cast<T*>(page_pointer + (vaddr & PAGE_MASK)) = data;
        return;
    }

    PageType type = current_page_table->attributes[vaddr >> PAGE_BITS];
    switch (type) {
    case PageType::Unmapped:
        LOG_ERROR(HW_Memory, "unmapped Write%lu 0x%08X @ 0x%08X", sizeof(data) * 8, (u32) data, vaddr);
        return;
    case PageType::Memory:
        ASSERT_MSG(false, "Mapped memory page without a pointer @ %08X", vaddr);
    case PageType::Special:
        LOG_ERROR(HW_Memory, "I/O writes aren't implemented yet @ %08X", vaddr);
        return;
    default:
        UNREACHABLE();
    }
}

u8* GetPointer(const VAddr vaddr) {
    u8* page_pointer = current_page_table->pointers[vaddr >> PAGE_BITS];
    if (page_pointer) {
        return page_pointer + (vaddr & PAGE_MASK);
    }

    LOG_ERROR(HW_Memory, "unknown GetPointer @ 0x%08x", vaddr);
    return nullptr;
}

u8* GetPhysicalPointer(PAddr address) {
    return GetPointer(PhysicalToVirtualAddress(address));
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
