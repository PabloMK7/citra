// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstring>
#include "audio_core/dsp_interface.h"
#include "common/assert.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/swap.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/lock.h"
#include "core/memory.h"
#include "core/memory_setup.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Memory {

static std::array<u8, Memory::VRAM_SIZE> vram;
static std::array<u8, Memory::N3DS_EXTRA_RAM_SIZE> n3ds_extra_ram;

static PageTable* current_page_table = nullptr;

void SetCurrentPageTable(PageTable* page_table) {
    current_page_table = page_table;
    if (Core::System::GetInstance().IsPoweredOn()) {
        Core::CPU().PageTableChanged();
    }
}

PageTable* GetCurrentPageTable() {
    return current_page_table;
}

static void MapPages(PageTable& page_table, u32 base, u32 size, u8* memory, PageType type) {
    LOG_DEBUG(HW_Memory, "Mapping {} onto {:08X}-{:08X}", (void*)memory, base * PAGE_SIZE,
              (base + size) * PAGE_SIZE);

    RasterizerFlushVirtualRegion(base << PAGE_BITS, size * PAGE_SIZE,
                                 FlushMode::FlushAndInvalidate);

    u32 end = base + size;
    while (base != end) {
        ASSERT_MSG(base < PAGE_TABLE_NUM_ENTRIES, "out of range mapping at {:08X}", base);

        page_table.attributes[base] = type;
        page_table.pointers[base] = memory;

        base += 1;
        if (memory != nullptr)
            memory += PAGE_SIZE;
    }
}

void MapMemoryRegion(PageTable& page_table, VAddr base, u32 size, u8* target) {
    ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: {:08X}", size);
    ASSERT_MSG((base & PAGE_MASK) == 0, "non-page aligned base: {:08X}", base);
    MapPages(page_table, base / PAGE_SIZE, size / PAGE_SIZE, target, PageType::Memory);
}

void MapIoRegion(PageTable& page_table, VAddr base, u32 size, MMIORegionPointer mmio_handler) {
    ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: {:08X}", size);
    ASSERT_MSG((base & PAGE_MASK) == 0, "non-page aligned base: {:08X}", base);
    MapPages(page_table, base / PAGE_SIZE, size / PAGE_SIZE, nullptr, PageType::Special);

    page_table.special_regions.emplace_back(SpecialRegion{base, size, mmio_handler});
}

void UnmapRegion(PageTable& page_table, VAddr base, u32 size) {
    ASSERT_MSG((size & PAGE_MASK) == 0, "non-page aligned size: {:08X}", size);
    ASSERT_MSG((base & PAGE_MASK) == 0, "non-page aligned base: {:08X}", base);
    MapPages(page_table, base / PAGE_SIZE, size / PAGE_SIZE, nullptr, PageType::Unmapped);
}

/**
 * Gets a pointer to the exact memory at the virtual address (i.e. not page aligned)
 * using a VMA from the current process
 */
static u8* GetPointerFromVMA(const Kernel::Process& process, VAddr vaddr) {
    u8* direct_pointer = nullptr;

    auto& vm_manager = process.vm_manager;

    auto it = vm_manager.FindVMA(vaddr);
    ASSERT(it != vm_manager.vma_map.end());

    auto& vma = it->second;
    switch (vma.type) {
    case Kernel::VMAType::AllocatedMemoryBlock:
        direct_pointer = vma.backing_block->data() + vma.offset;
        break;
    case Kernel::VMAType::BackingMemory:
        direct_pointer = vma.backing_memory;
        break;
    case Kernel::VMAType::Free:
        return nullptr;
    default:
        UNREACHABLE();
    }

    return direct_pointer + (vaddr - vma.base);
}

/**
 * Gets a pointer to the exact memory at the virtual address (i.e. not page aligned)
 * using a VMA from the current process.
 */
static u8* GetPointerFromVMA(VAddr vaddr) {
    return GetPointerFromVMA(*Kernel::g_current_process, vaddr);
}

/**
 * This function should only be called for virtual addreses with attribute `PageType::Special`.
 */
static MMIORegionPointer GetMMIOHandler(const PageTable& page_table, VAddr vaddr) {
    for (const auto& region : page_table.special_regions) {
        if (vaddr >= region.base && vaddr < (region.base + region.size)) {
            return region.handler;
        }
    }
    ASSERT_MSG(false, "Mapped IO page without a handler @ {:08X}", vaddr);
    return nullptr; // Should never happen
}

static MMIORegionPointer GetMMIOHandler(VAddr vaddr) {
    const PageTable& page_table = Kernel::g_current_process->vm_manager.page_table;
    return GetMMIOHandler(page_table, vaddr);
}

template <typename T>
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

    // The memory access might do an MMIO or cached access, so we have to lock the HLE kernel state
    std::lock_guard<std::recursive_mutex> lock(HLE::g_hle_lock);

    PageType type = current_page_table->attributes[vaddr >> PAGE_BITS];
    switch (type) {
    case PageType::Unmapped:
        LOG_ERROR(HW_Memory, "unmapped Read{} @ 0x{:08X}", sizeof(T) * 8, vaddr);
        return 0;
    case PageType::Memory:
        ASSERT_MSG(false, "Mapped memory page without a pointer @ {:08X}", vaddr);
        break;
    case PageType::RasterizerCachedMemory: {
        RasterizerFlushVirtualRegion(vaddr, sizeof(T), FlushMode::Flush);

        T value;
        std::memcpy(&value, GetPointerFromVMA(vaddr), sizeof(T));
        return value;
    }
    case PageType::Special:
        return ReadMMIO<T>(GetMMIOHandler(vaddr), vaddr);
    default:
        UNREACHABLE();
    }
}

template <typename T>
void WriteMMIO(MMIORegionPointer mmio_handler, VAddr addr, const T data);

template <typename T>
void Write(const VAddr vaddr, const T data) {
    u8* page_pointer = current_page_table->pointers[vaddr >> PAGE_BITS];
    if (page_pointer) {
        // NOTE: Avoid adding any extra logic to this fast-path block
        std::memcpy(&page_pointer[vaddr & PAGE_MASK], &data, sizeof(T));
        return;
    }

    // The memory access might do an MMIO or cached access, so we have to lock the HLE kernel state
    std::lock_guard<std::recursive_mutex> lock(HLE::g_hle_lock);

    PageType type = current_page_table->attributes[vaddr >> PAGE_BITS];
    switch (type) {
    case PageType::Unmapped:
        LOG_ERROR(HW_Memory, "unmapped Write{} 0x{:08X} @ 0x{:08X}", sizeof(data) * 8, (u32)data,
                  vaddr);
        return;
    case PageType::Memory:
        ASSERT_MSG(false, "Mapped memory page without a pointer @ {:08X}", vaddr);
        break;
    case PageType::RasterizerCachedMemory: {
        RasterizerFlushVirtualRegion(vaddr, sizeof(T), FlushMode::Invalidate);
        std::memcpy(GetPointerFromVMA(vaddr), &data, sizeof(T));
        break;
    }
    case PageType::Special:
        WriteMMIO<T>(GetMMIOHandler(vaddr), vaddr, data);
        break;
    default:
        UNREACHABLE();
    }
}

bool IsValidVirtualAddress(const Kernel::Process& process, const VAddr vaddr) {
    auto& page_table = process.vm_manager.page_table;

    const u8* page_pointer = page_table.pointers[vaddr >> PAGE_BITS];
    if (page_pointer)
        return true;

    if (page_table.attributes[vaddr >> PAGE_BITS] == PageType::RasterizerCachedMemory)
        return true;

    if (page_table.attributes[vaddr >> PAGE_BITS] != PageType::Special)
        return false;

    MMIORegionPointer mmio_region = GetMMIOHandler(page_table, vaddr);
    if (mmio_region) {
        return mmio_region->IsValidAddress(vaddr);
    }

    return false;
}

bool IsValidVirtualAddress(const VAddr vaddr) {
    return IsValidVirtualAddress(*Kernel::g_current_process, vaddr);
}

bool IsValidPhysicalAddress(const PAddr paddr) {
    return GetPhysicalPointer(paddr) != nullptr;
}

u8* GetPointer(const VAddr vaddr) {
    u8* page_pointer = current_page_table->pointers[vaddr >> PAGE_BITS];
    if (page_pointer) {
        return page_pointer + (vaddr & PAGE_MASK);
    }

    if (current_page_table->attributes[vaddr >> PAGE_BITS] == PageType::RasterizerCachedMemory) {
        return GetPointerFromVMA(vaddr);
    }

    LOG_ERROR(HW_Memory, "unknown GetPointer @ 0x{:08x}", vaddr);
    return nullptr;
}

std::string ReadCString(VAddr vaddr, std::size_t max_length) {
    std::string string;
    string.reserve(max_length);
    for (std::size_t i = 0; i < max_length; ++i) {
        char c = Read8(vaddr);
        if (c == '\0')
            break;
        string.push_back(c);
        ++vaddr;
    }
    string.shrink_to_fit();
    return string;
}

u8* GetPhysicalPointer(PAddr address) {
    struct MemoryArea {
        PAddr paddr_base;
        u32 size;
    };

    static constexpr MemoryArea memory_areas[] = {
        {VRAM_PADDR, VRAM_SIZE},
        {IO_AREA_PADDR, IO_AREA_SIZE},
        {DSP_RAM_PADDR, DSP_RAM_SIZE},
        {FCRAM_PADDR, FCRAM_N3DS_SIZE},
        {N3DS_EXTRA_RAM_PADDR, N3DS_EXTRA_RAM_SIZE},
    };

    const auto area =
        std::find_if(std::begin(memory_areas), std::end(memory_areas), [&](const auto& area) {
            return address >= area.paddr_base && address < area.paddr_base + area.size;
        });

    if (area == std::end(memory_areas)) {
        LOG_ERROR(HW_Memory, "unknown GetPhysicalPointer @ 0x{:08X}", address);
        return nullptr;
    }

    if (area->paddr_base == IO_AREA_PADDR) {
        LOG_ERROR(HW_Memory, "MMIO mappings are not supported yet. phys_addr=0x{:08X}", address);
        return nullptr;
    }

    u32 offset_into_region = address - area->paddr_base;

    u8* target_pointer = nullptr;
    switch (area->paddr_base) {
    case VRAM_PADDR:
        target_pointer = vram.data() + offset_into_region;
        break;
    case DSP_RAM_PADDR:
        target_pointer = Core::DSP().GetDspMemory().data() + offset_into_region;
        break;
    case FCRAM_PADDR:
        for (const auto& region : Kernel::memory_regions) {
            if (offset_into_region >= region.base &&
                offset_into_region < region.base + region.size) {
                target_pointer =
                    region.linear_heap_memory->data() + offset_into_region - region.base;
                break;
            }
        }
        ASSERT_MSG(target_pointer != nullptr, "Invalid FCRAM address");
        break;
    case N3DS_EXTRA_RAM_PADDR:
        target_pointer = n3ds_extra_ram.data() + offset_into_region;
        break;
    default:
        UNREACHABLE();
    }

    return target_pointer;
}

void RasterizerMarkRegionCached(PAddr start, u32 size, bool cached) {
    if (start == 0) {
        return;
    }

    u32 num_pages = ((start + size - 1) >> PAGE_BITS) - (start >> PAGE_BITS) + 1;
    PAddr paddr = start;

    for (unsigned i = 0; i < num_pages; ++i, paddr += PAGE_SIZE) {
        boost::optional<VAddr> maybe_vaddr = PhysicalToVirtualAddress(paddr);
        // While the physical <-> virtual mapping is 1:1 for the regions supported by the cache,
        // some games (like Pokemon Super Mystery Dungeon) will try to use textures that go beyond
        // the end address of VRAM, causing the Virtual->Physical translation to fail when flushing
        // parts of the texture.
        if (!maybe_vaddr) {
            LOG_ERROR(HW_Memory,
                      "Trying to flush a cached region to an invalid physical address {:08X}",
                      paddr);
            continue;
        }
        VAddr vaddr = *maybe_vaddr;

        PageType& page_type = current_page_table->attributes[vaddr >> PAGE_BITS];

        if (cached) {
            // Switch page type to cached if now cached
            switch (page_type) {
            case PageType::Unmapped:
                // It is not necessary for a process to have this region mapped into its address
                // space, for example, a system module need not have a VRAM mapping.
                break;
            case PageType::Memory:
                page_type = PageType::RasterizerCachedMemory;
                current_page_table->pointers[vaddr >> PAGE_BITS] = nullptr;
                break;
            default:
                UNREACHABLE();
            }
        } else {
            // Switch page type to uncached if now uncached
            switch (page_type) {
            case PageType::Unmapped:
                // It is not necessary for a process to have this region mapped into its address
                // space, for example, a system module need not have a VRAM mapping.
                break;
            case PageType::RasterizerCachedMemory: {
                u8* pointer = GetPointerFromVMA(vaddr & ~PAGE_MASK);
                if (pointer == nullptr) {
                    // It's possible that this function has been called while updating the pagetable
                    // after unmapping a VMA. In that case the underlying VMA will no longer exist,
                    // and we should just leave the pagetable entry blank.
                    page_type = PageType::Unmapped;
                } else {
                    page_type = PageType::Memory;
                    current_page_table->pointers[vaddr >> PAGE_BITS] = pointer;
                }
                break;
            }
            default:
                UNREACHABLE();
            }
        }
    }
}

void RasterizerFlushRegion(PAddr start, u32 size) {
    if (VideoCore::g_renderer == nullptr) {
        return;
    }

    VideoCore::g_renderer->Rasterizer()->FlushRegion(start, size);
}

void RasterizerInvalidateRegion(PAddr start, u32 size) {
    if (VideoCore::g_renderer == nullptr) {
        return;
    }

    VideoCore::g_renderer->Rasterizer()->InvalidateRegion(start, size);
}

void RasterizerFlushAndInvalidateRegion(PAddr start, u32 size) {
    // Since pages are unmapped on shutdown after video core is shutdown, the renderer may be
    // null here
    if (VideoCore::g_renderer == nullptr) {
        return;
    }

    VideoCore::g_renderer->Rasterizer()->FlushAndInvalidateRegion(start, size);
}

void RasterizerFlushVirtualRegion(VAddr start, u32 size, FlushMode mode) {
    // Since pages are unmapped on shutdown after video core is shutdown, the renderer may be
    // null here
    if (VideoCore::g_renderer == nullptr) {
        return;
    }

    VAddr end = start + size;

    auto CheckRegion = [&](VAddr region_start, VAddr region_end) {
        if (start >= region_end || end <= region_start) {
            // No overlap with region
            return;
        }

        VAddr overlap_start = std::max(start, region_start);
        VAddr overlap_end = std::min(end, region_end);

        PAddr physical_start = TryVirtualToPhysicalAddress(overlap_start).value();
        u32 overlap_size = overlap_end - overlap_start;

        auto* rasterizer = VideoCore::g_renderer->Rasterizer();
        switch (mode) {
        case FlushMode::Flush:
            rasterizer->FlushRegion(physical_start, overlap_size);
            break;
        case FlushMode::Invalidate:
            rasterizer->InvalidateRegion(physical_start, overlap_size);
            break;
        case FlushMode::FlushAndInvalidate:
            rasterizer->FlushAndInvalidateRegion(physical_start, overlap_size);
            break;
        }
    };

    CheckRegion(LINEAR_HEAP_VADDR, LINEAR_HEAP_VADDR_END);
    CheckRegion(NEW_LINEAR_HEAP_VADDR, NEW_LINEAR_HEAP_VADDR_END);
    CheckRegion(VRAM_VADDR, VRAM_VADDR_END);
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

void ReadBlock(const Kernel::Process& process, const VAddr src_addr, void* dest_buffer,
               const std::size_t size) {
    auto& page_table = process.vm_manager.page_table;

    std::size_t remaining_size = size;
    std::size_t page_index = src_addr >> PAGE_BITS;
    std::size_t page_offset = src_addr & PAGE_MASK;

    while (remaining_size > 0) {
        const std::size_t copy_amount = std::min(PAGE_SIZE - page_offset, remaining_size);
        const VAddr current_vaddr = static_cast<VAddr>((page_index << PAGE_BITS) + page_offset);

        switch (page_table.attributes[page_index]) {
        case PageType::Unmapped: {
            LOG_ERROR(HW_Memory,
                      "unmapped ReadBlock @ 0x{:08X} (start address = 0x{:08X}, size = {})",
                      current_vaddr, src_addr, size);
            std::memset(dest_buffer, 0, copy_amount);
            break;
        }
        case PageType::Memory: {
            DEBUG_ASSERT(page_table.pointers[page_index]);

            const u8* src_ptr = page_table.pointers[page_index] + page_offset;
            std::memcpy(dest_buffer, src_ptr, copy_amount);
            break;
        }
        case PageType::Special: {
            MMIORegionPointer handler = GetMMIOHandler(page_table, current_vaddr);
            DEBUG_ASSERT(handler);
            handler->ReadBlock(current_vaddr, dest_buffer, copy_amount);
            break;
        }
        case PageType::RasterizerCachedMemory: {
            RasterizerFlushVirtualRegion(current_vaddr, static_cast<u32>(copy_amount),
                                         FlushMode::Flush);
            std::memcpy(dest_buffer, GetPointerFromVMA(process, current_vaddr), copy_amount);
            break;
        }
        default:
            UNREACHABLE();
        }

        page_index++;
        page_offset = 0;
        dest_buffer = static_cast<u8*>(dest_buffer) + copy_amount;
        remaining_size -= copy_amount;
    }
}

void ReadBlock(const VAddr src_addr, void* dest_buffer, const std::size_t size) {
    ReadBlock(*Kernel::g_current_process, src_addr, dest_buffer, size);
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

void WriteBlock(const Kernel::Process& process, const VAddr dest_addr, const void* src_buffer,
                const std::size_t size) {
    auto& page_table = process.vm_manager.page_table;
    std::size_t remaining_size = size;
    std::size_t page_index = dest_addr >> PAGE_BITS;
    std::size_t page_offset = dest_addr & PAGE_MASK;

    while (remaining_size > 0) {
        const std::size_t copy_amount = std::min(PAGE_SIZE - page_offset, remaining_size);
        const VAddr current_vaddr = static_cast<VAddr>((page_index << PAGE_BITS) + page_offset);

        switch (page_table.attributes[page_index]) {
        case PageType::Unmapped: {
            LOG_ERROR(HW_Memory,
                      "unmapped WriteBlock @ 0x{:08X} (start address = 0x{:08X}, size = {})",
                      current_vaddr, dest_addr, size);
            break;
        }
        case PageType::Memory: {
            DEBUG_ASSERT(page_table.pointers[page_index]);

            u8* dest_ptr = page_table.pointers[page_index] + page_offset;
            std::memcpy(dest_ptr, src_buffer, copy_amount);
            break;
        }
        case PageType::Special: {
            MMIORegionPointer handler = GetMMIOHandler(page_table, current_vaddr);
            DEBUG_ASSERT(handler);
            handler->WriteBlock(current_vaddr, src_buffer, copy_amount);
            break;
        }
        case PageType::RasterizerCachedMemory: {
            RasterizerFlushVirtualRegion(current_vaddr, static_cast<u32>(copy_amount),
                                         FlushMode::Invalidate);
            std::memcpy(GetPointerFromVMA(process, current_vaddr), src_buffer, copy_amount);
            break;
        }
        default:
            UNREACHABLE();
        }

        page_index++;
        page_offset = 0;
        src_buffer = static_cast<const u8*>(src_buffer) + copy_amount;
        remaining_size -= copy_amount;
    }
}

void WriteBlock(const VAddr dest_addr, const void* src_buffer, const std::size_t size) {
    WriteBlock(*Kernel::g_current_process, dest_addr, src_buffer, size);
}

void ZeroBlock(const Kernel::Process& process, const VAddr dest_addr, const std::size_t size) {
    auto& page_table = process.vm_manager.page_table;
    std::size_t remaining_size = size;
    std::size_t page_index = dest_addr >> PAGE_BITS;
    std::size_t page_offset = dest_addr & PAGE_MASK;

    static const std::array<u8, PAGE_SIZE> zeros = {};

    while (remaining_size > 0) {
        const std::size_t copy_amount = std::min(PAGE_SIZE - page_offset, remaining_size);
        const VAddr current_vaddr = static_cast<VAddr>((page_index << PAGE_BITS) + page_offset);

        switch (page_table.attributes[page_index]) {
        case PageType::Unmapped: {
            LOG_ERROR(HW_Memory,
                      "unmapped ZeroBlock @ 0x{:08X} (start address = 0x{:08X}, size = {})",
                      current_vaddr, dest_addr, size);
            break;
        }
        case PageType::Memory: {
            DEBUG_ASSERT(page_table.pointers[page_index]);

            u8* dest_ptr = page_table.pointers[page_index] + page_offset;
            std::memset(dest_ptr, 0, copy_amount);
            break;
        }
        case PageType::Special: {
            MMIORegionPointer handler = GetMMIOHandler(page_table, current_vaddr);
            DEBUG_ASSERT(handler);
            handler->WriteBlock(current_vaddr, zeros.data(), copy_amount);
            break;
        }
        case PageType::RasterizerCachedMemory: {
            RasterizerFlushVirtualRegion(current_vaddr, static_cast<u32>(copy_amount),
                                         FlushMode::Invalidate);
            std::memset(GetPointerFromVMA(process, current_vaddr), 0, copy_amount);
            break;
        }
        default:
            UNREACHABLE();
        }

        page_index++;
        page_offset = 0;
        remaining_size -= copy_amount;
    }
}

void ZeroBlock(const VAddr dest_addr, const std::size_t size) {
    ZeroBlock(*Kernel::g_current_process, dest_addr, size);
}

void CopyBlock(const Kernel::Process& process, VAddr dest_addr, VAddr src_addr,
               const std::size_t size) {
    auto& page_table = process.vm_manager.page_table;
    std::size_t remaining_size = size;
    std::size_t page_index = src_addr >> PAGE_BITS;
    std::size_t page_offset = src_addr & PAGE_MASK;

    while (remaining_size > 0) {
        const std::size_t copy_amount = std::min(PAGE_SIZE - page_offset, remaining_size);
        const VAddr current_vaddr = static_cast<VAddr>((page_index << PAGE_BITS) + page_offset);

        switch (page_table.attributes[page_index]) {
        case PageType::Unmapped: {
            LOG_ERROR(HW_Memory,
                      "unmapped CopyBlock @ 0x{:08X} (start address = 0x{:08X}, size = {})",
                      current_vaddr, src_addr, size);
            ZeroBlock(process, dest_addr, copy_amount);
            break;
        }
        case PageType::Memory: {
            DEBUG_ASSERT(page_table.pointers[page_index]);
            const u8* src_ptr = page_table.pointers[page_index] + page_offset;
            WriteBlock(process, dest_addr, src_ptr, copy_amount);
            break;
        }
        case PageType::Special: {
            MMIORegionPointer handler = GetMMIOHandler(page_table, current_vaddr);
            DEBUG_ASSERT(handler);
            std::vector<u8> buffer(copy_amount);
            handler->ReadBlock(current_vaddr, buffer.data(), buffer.size());
            WriteBlock(process, dest_addr, buffer.data(), buffer.size());
            break;
        }
        case PageType::RasterizerCachedMemory: {
            RasterizerFlushVirtualRegion(current_vaddr, static_cast<u32>(copy_amount),
                                         FlushMode::Flush);
            WriteBlock(process, dest_addr, GetPointerFromVMA(process, current_vaddr), copy_amount);
            break;
        }
        default:
            UNREACHABLE();
        }

        page_index++;
        page_offset = 0;
        dest_addr += static_cast<VAddr>(copy_amount);
        src_addr += static_cast<VAddr>(copy_amount);
        remaining_size -= copy_amount;
    }
}

void CopyBlock(VAddr dest_addr, VAddr src_addr, const std::size_t size) {
    CopyBlock(*Kernel::g_current_process, dest_addr, src_addr, size);
}

template <>
u8 ReadMMIO<u8>(MMIORegionPointer mmio_handler, VAddr addr) {
    return mmio_handler->Read8(addr);
}

template <>
u16 ReadMMIO<u16>(MMIORegionPointer mmio_handler, VAddr addr) {
    return mmio_handler->Read16(addr);
}

template <>
u32 ReadMMIO<u32>(MMIORegionPointer mmio_handler, VAddr addr) {
    return mmio_handler->Read32(addr);
}

template <>
u64 ReadMMIO<u64>(MMIORegionPointer mmio_handler, VAddr addr) {
    return mmio_handler->Read64(addr);
}

template <>
void WriteMMIO<u8>(MMIORegionPointer mmio_handler, VAddr addr, const u8 data) {
    mmio_handler->Write8(addr, data);
}

template <>
void WriteMMIO<u16>(MMIORegionPointer mmio_handler, VAddr addr, const u16 data) {
    mmio_handler->Write16(addr, data);
}

template <>
void WriteMMIO<u32>(MMIORegionPointer mmio_handler, VAddr addr, const u32 data) {
    mmio_handler->Write32(addr, data);
}

template <>
void WriteMMIO<u64>(MMIORegionPointer mmio_handler, VAddr addr, const u64 data) {
    mmio_handler->Write64(addr, data);
}

boost::optional<PAddr> TryVirtualToPhysicalAddress(const VAddr addr) {
    if (addr == 0) {
        return 0;
    } else if (addr >= VRAM_VADDR && addr < VRAM_VADDR_END) {
        return addr - VRAM_VADDR + VRAM_PADDR;
    } else if (addr >= LINEAR_HEAP_VADDR && addr < LINEAR_HEAP_VADDR_END) {
        return addr - LINEAR_HEAP_VADDR + FCRAM_PADDR;
    } else if (addr >= NEW_LINEAR_HEAP_VADDR && addr < NEW_LINEAR_HEAP_VADDR_END) {
        return addr - NEW_LINEAR_HEAP_VADDR + FCRAM_PADDR;
    } else if (addr >= DSP_RAM_VADDR && addr < DSP_RAM_VADDR_END) {
        return addr - DSP_RAM_VADDR + DSP_RAM_PADDR;
    } else if (addr >= IO_AREA_VADDR && addr < IO_AREA_VADDR_END) {
        return addr - IO_AREA_VADDR + IO_AREA_PADDR;
    } else if (addr >= N3DS_EXTRA_RAM_VADDR && addr < N3DS_EXTRA_RAM_VADDR_END) {
        return addr - N3DS_EXTRA_RAM_VADDR + N3DS_EXTRA_RAM_PADDR;
    }

    return boost::none;
}

PAddr VirtualToPhysicalAddress(const VAddr addr) {
    auto paddr = TryVirtualToPhysicalAddress(addr);
    if (!paddr) {
        LOG_ERROR(HW_Memory, "Unknown virtual address @ 0x{:08X}", addr);
        // To help with debugging, set bit on address so that it's obviously invalid.
        return addr | 0x80000000;
    }
    return *paddr;
}

boost::optional<VAddr> PhysicalToVirtualAddress(const PAddr addr) {
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
    } else if (addr >= N3DS_EXTRA_RAM_PADDR && addr < N3DS_EXTRA_RAM_PADDR_END) {
        return addr - N3DS_EXTRA_RAM_PADDR + N3DS_EXTRA_RAM_VADDR;
    }

    return boost::none;
}

} // namespace Memory
