// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/logging/log.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/memory.h"

namespace Kernel {

SharedMemory::SharedMemory(KernelSystem& kernel) : Object(kernel) {}
SharedMemory::~SharedMemory() {}

SharedPtr<SharedMemory> KernelSystem::CreateSharedMemory(Process* owner_process, u32 size,
                                                         MemoryPermission permissions,
                                                         MemoryPermission other_permissions,
                                                         VAddr address, MemoryRegion region,
                                                         std::string name) {
    SharedPtr<SharedMemory> shared_memory(new SharedMemory(*this));

    shared_memory->owner_process = owner_process;
    shared_memory->name = std::move(name);
    shared_memory->size = size;
    shared_memory->permissions = permissions;
    shared_memory->other_permissions = other_permissions;

    if (address == 0) {
        // We need to allocate a block from the Linear Heap ourselves.
        // We'll manually allocate some memory from the linear heap in the specified region.
        MemoryRegionInfo* memory_region = GetMemoryRegion(region);
        auto& linheap_memory = memory_region->linear_heap_memory;

        ASSERT_MSG(linheap_memory->size() + size <= memory_region->size,
                   "Not enough space in region to allocate shared memory!");

        shared_memory->backing_block = linheap_memory;
        shared_memory->backing_block_offset = linheap_memory->size();
        // Allocate some memory from the end of the linear heap for this region.
        linheap_memory->insert(linheap_memory->end(), size, 0);
        memory_region->used += size;

        shared_memory->linear_heap_phys_address =
            Memory::FCRAM_PADDR + memory_region->base +
            static_cast<PAddr>(shared_memory->backing_block_offset);

        // Increase the amount of used linear heap memory for the owner process.
        if (shared_memory->owner_process != nullptr) {
            shared_memory->owner_process->linear_heap_used += size;
        }

        // Refresh the address mappings for the current process.
        if (current_process != nullptr) {
            current_process->vm_manager.RefreshMemoryBlockMappings(linheap_memory.get());
        }
    } else {
        auto& vm_manager = shared_memory->owner_process->vm_manager;
        // The memory is already available and mapped in the owner process.
        auto vma = vm_manager.FindVMA(address);
        ASSERT_MSG(vma != vm_manager.vma_map.end(), "Invalid memory address");
        ASSERT_MSG(vma->second.backing_block, "Backing block doesn't exist for address");

        // The returned VMA might be a bigger one encompassing the desired address.
        auto vma_offset = address - vma->first;
        ASSERT_MSG(vma_offset + size <= vma->second.size,
                   "Shared memory exceeds bounds of mapped block");

        shared_memory->backing_block = vma->second.backing_block;
        shared_memory->backing_block_offset = vma->second.offset + vma_offset;
    }

    shared_memory->base_address = address;
    return shared_memory;
}

SharedPtr<SharedMemory> KernelSystem::CreateSharedMemoryForApplet(
    std::shared_ptr<std::vector<u8>> heap_block, u32 offset, u32 size, MemoryPermission permissions,
    MemoryPermission other_permissions, std::string name) {
    SharedPtr<SharedMemory> shared_memory(new SharedMemory(*this));

    shared_memory->owner_process = nullptr;
    shared_memory->name = std::move(name);
    shared_memory->size = size;
    shared_memory->permissions = permissions;
    shared_memory->other_permissions = other_permissions;
    shared_memory->backing_block = heap_block;
    shared_memory->backing_block_offset = offset;
    shared_memory->base_address = Memory::HEAP_VADDR + offset;

    return shared_memory;
}

ResultCode SharedMemory::Map(Process* target_process, VAddr address, MemoryPermission permissions,
                             MemoryPermission other_permissions) {

    MemoryPermission own_other_permissions =
        target_process == owner_process ? this->permissions : this->other_permissions;

    // Automatically allocated memory blocks can only be mapped with other_permissions = DontCare
    if (base_address == 0 && other_permissions != MemoryPermission::DontCare) {
        return ERR_INVALID_COMBINATION;
    }

    // Error out if the requested permissions don't match what the creator process allows.
    if (static_cast<u32>(permissions) & ~static_cast<u32>(own_other_permissions)) {
        LOG_ERROR(Kernel, "cannot map id={}, address=0x{:08X} name={}, permissions don't match",
                  GetObjectId(), address, name);
        return ERR_INVALID_COMBINATION;
    }

    // Heap-backed memory blocks can not be mapped with other_permissions = DontCare
    if (base_address != 0 && other_permissions == MemoryPermission::DontCare) {
        LOG_ERROR(Kernel, "cannot map id={}, address=0x{08X} name={}, permissions don't match",
                  GetObjectId(), address, name);
        return ERR_INVALID_COMBINATION;
    }

    // Error out if the provided permissions are not compatible with what the creator process needs.
    if (other_permissions != MemoryPermission::DontCare &&
        static_cast<u32>(this->permissions) & ~static_cast<u32>(other_permissions)) {
        LOG_ERROR(Kernel, "cannot map id={}, address=0x{:08X} name={}, permissions don't match",
                  GetObjectId(), address, name);
        return ERR_WRONG_PERMISSION;
    }

    // TODO(Subv): Check for the Shared Device Mem flag in the creator process.
    /*if (was_created_with_shared_device_mem && address != 0) {
        return ResultCode(ErrorDescription::InvalidCombination, ErrorModule::OS,
    ErrorSummary::InvalidArgument, ErrorLevel::Usage);
    }*/

    // TODO(Subv): The same process that created a SharedMemory object
    // can not map it in its own address space unless it was created with addr=0, result 0xD900182C.

    if (address != 0) {
        if (address < Memory::HEAP_VADDR || address + size >= Memory::SHARED_MEMORY_VADDR_END) {
            LOG_ERROR(Kernel, "cannot map id={}, address=0x{:08X} name={}, invalid address",
                      GetObjectId(), address, name);
            return ERR_INVALID_ADDRESS;
        }
    }

    VAddr target_address = address;

    if (base_address == 0 && target_address == 0) {
        // Calculate the address at which to map the memory block.
        auto maybe_vaddr = Memory::PhysicalToVirtualAddress(linear_heap_phys_address);
        ASSERT(maybe_vaddr);
        target_address = *maybe_vaddr;
    }

    // Map the memory block into the target process
    auto result = target_process->vm_manager.MapMemoryBlock(
        target_address, backing_block, backing_block_offset, size, MemoryState::Shared);
    if (result.Failed()) {
        LOG_ERROR(
            Kernel,
            "cannot map id={}, target_address=0x{:08X} name={}, error mapping to virtual memory",
            GetObjectId(), target_address, name);
        return result.Code();
    }

    return target_process->vm_manager.ReprotectRange(target_address, size,
                                                     ConvertPermissions(permissions));
}

ResultCode SharedMemory::Unmap(Process* target_process, VAddr address) {
    // TODO(Subv): Verify what happens if the application tries to unmap an address that is not
    // mapped to a SharedMemory.
    return target_process->vm_manager.UnmapRange(address, size);
}

VMAPermission SharedMemory::ConvertPermissions(MemoryPermission permission) {
    u32 masked_permissions =
        static_cast<u32>(permission) & static_cast<u32>(MemoryPermission::ReadWriteExecute);
    return static_cast<VMAPermission>(masked_permissions);
};

u8* SharedMemory::GetPointer(u32 offset) {
    return backing_block->data() + backing_block_offset + offset;
}

} // namespace Kernel
