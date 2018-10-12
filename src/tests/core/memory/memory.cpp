// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <catch2/catch.hpp>
#include "core/core_timing.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/process.h"
#include "core/hle/shared_page.h"
#include "core/memory.h"

TEST_CASE("Memory::IsValidVirtualAddress", "[core][memory]") {
    CoreTiming::Init();
    Kernel::KernelSystem kernel(0);
    SECTION("these regions should not be mapped on an empty process") {
        auto process = kernel.CreateProcess(kernel.CreateCodeSet("", 0));
        CHECK(Memory::IsValidVirtualAddress(*process, Memory::PROCESS_IMAGE_VADDR) == false);
        CHECK(Memory::IsValidVirtualAddress(*process, Memory::HEAP_VADDR) == false);
        CHECK(Memory::IsValidVirtualAddress(*process, Memory::LINEAR_HEAP_VADDR) == false);
        CHECK(Memory::IsValidVirtualAddress(*process, Memory::VRAM_VADDR) == false);
        CHECK(Memory::IsValidVirtualAddress(*process, Memory::CONFIG_MEMORY_VADDR) == false);
        CHECK(Memory::IsValidVirtualAddress(*process, Memory::SHARED_PAGE_VADDR) == false);
        CHECK(Memory::IsValidVirtualAddress(*process, Memory::TLS_AREA_VADDR) == false);
    }

    SECTION("CONFIG_MEMORY_VADDR and SHARED_PAGE_VADDR should be valid after mapping them") {
        auto process = kernel.CreateProcess(kernel.CreateCodeSet("", 0));
        Kernel::MapSharedPages(process->vm_manager);
        CHECK(Memory::IsValidVirtualAddress(*process, Memory::CONFIG_MEMORY_VADDR) == true);
        CHECK(Memory::IsValidVirtualAddress(*process, Memory::SHARED_PAGE_VADDR) == true);
    }

    SECTION("special regions should be valid after mapping them") {
        auto process = kernel.CreateProcess(kernel.CreateCodeSet("", 0));
        SECTION("VRAM") {
            Kernel::HandleSpecialMapping(process->vm_manager,
                                         {Memory::VRAM_VADDR, Memory::VRAM_SIZE, false, false});
            CHECK(Memory::IsValidVirtualAddress(*process, Memory::VRAM_VADDR) == true);
        }

        SECTION("IO (Not yet implemented)") {
            Kernel::HandleSpecialMapping(
                process->vm_manager, {Memory::IO_AREA_VADDR, Memory::IO_AREA_SIZE, false, false});
            CHECK_FALSE(Memory::IsValidVirtualAddress(*process, Memory::IO_AREA_VADDR) == true);
        }
    }

    SECTION("Unmapping a VAddr should make it invalid") {
        auto process = kernel.CreateProcess(kernel.CreateCodeSet("", 0));
        Kernel::MapSharedPages(process->vm_manager);
        process->vm_manager.UnmapRange(Memory::CONFIG_MEMORY_VADDR, Memory::CONFIG_MEMORY_SIZE);
        CHECK(Memory::IsValidVirtualAddress(*process, Memory::CONFIG_MEMORY_VADDR) == false);
    }

    CoreTiming::Shutdown();
}
