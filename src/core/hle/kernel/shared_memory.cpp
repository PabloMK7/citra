// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/logging/log.h"

#include "core/memory.h"
#include "core/hle/kernel/shared_memory.h"

namespace Kernel {

SharedMemory::SharedMemory() {}
SharedMemory::~SharedMemory() {}

SharedPtr<SharedMemory> SharedMemory::Create(u32 size, MemoryPermission permissions,
        MemoryPermission other_permissions, std::string name) {
    SharedPtr<SharedMemory> shared_memory(new SharedMemory);

    shared_memory->name = std::move(name);
    shared_memory->base_address = 0x0;
    shared_memory->fixed_address = 0x0;
    shared_memory->size = size;
    shared_memory->permissions = permissions;
    shared_memory->other_permissions = other_permissions;

    return shared_memory;
}

ResultCode SharedMemory::Map(VAddr address, MemoryPermission permissions,
        MemoryPermission other_permissions) {

    if (base_address != 0) {
        LOG_ERROR(Kernel, "cannot map id=%u, address=0x%08X name=%s: already mapped at 0x%08X!",
            GetObjectId(), address, name.c_str(), base_address);
        // TODO: Verify error code with hardware
        return ResultCode(ErrorDescription::InvalidAddress, ErrorModule::Kernel,
            ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
    }

    // TODO(Subv): Return E0E01BEE when permissions and other_permissions don't
    // match what was specified when the memory block was created.

    // TODO(Subv): Return E0E01BEE when address should be 0.
    // Note: Find out when that's the case.

    if (fixed_address != 0) {
         if (address != 0 && address != fixed_address) {
            LOG_ERROR(Kernel, "cannot map id=%u, address=0x%08X name=%s: fixed_addres is 0x%08X!",
                    GetObjectId(), address, name.c_str(), fixed_address);
            // TODO: Verify error code with hardware
            return ResultCode(ErrorDescription::InvalidAddress, ErrorModule::Kernel,
                ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
        }

        // HACK(yuriks): This is only here to support the APT shared font mapping right now.
        // Later, this should actually map the memory block onto the address space.
        return RESULT_SUCCESS;
    }

    if (address < Memory::SHARED_MEMORY_VADDR || address + size >= Memory::SHARED_MEMORY_VADDR_END) {
        LOG_ERROR(Kernel, "cannot map id=%u, address=0x%08X name=%s outside of shared mem bounds!",
                GetObjectId(), address, name.c_str());
        // TODO: Verify error code with hardware
        return ResultCode(ErrorDescription::InvalidAddress, ErrorModule::Kernel,
                ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
    }

    // TODO: Test permissions

    // HACK: Since there's no way to write to the memory block without mapping it onto the game
    // process yet, at least initialize memory the first time it's mapped.
    if (address != this->base_address) {
        std::memset(Memory::GetPointer(address), 0, size);
    }

    this->base_address = address;

    return RESULT_SUCCESS;
}

ResultCode SharedMemory::Unmap(VAddr address) {
    if (base_address == 0) {
        // TODO(Subv): Verify what actually happens when you want to unmap a memory block that
        // was originally mapped with address = 0
        return ResultCode(ErrorDescription::InvalidAddress, ErrorModule::OS, ErrorSummary::InvalidArgument, ErrorLevel::Usage);
    }

    if (base_address != address)
        return ResultCode(ErrorDescription::WrongAddress, ErrorModule::OS, ErrorSummary::InvalidState, ErrorLevel::Usage);

    base_address = 0;

    return RESULT_SUCCESS;
}

u8* SharedMemory::GetPointer(u32 offset) {
    if (base_address != 0)
        return Memory::GetPointer(base_address + offset);

    LOG_ERROR(Kernel_SVC, "memory block id=%u not mapped!", GetObjectId());
    return nullptr;
}

} // namespace
