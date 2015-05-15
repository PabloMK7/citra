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
    shared_memory->size = size;
    shared_memory->permissions = permissions;
    shared_memory->other_permissions = other_permissions;

    return shared_memory;
}

ResultCode SharedMemory::Map(VAddr address, MemoryPermission permissions,
        MemoryPermission other_permissions) {

    if (address < Memory::SHARED_MEMORY_VADDR || address + size >= Memory::SHARED_MEMORY_VADDR_END) {
        LOG_ERROR(Kernel, "cannot map id=%u, address=0x%08X outside of shared mem bounds!",
                GetObjectId(), address);
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

u8* SharedMemory::GetPointer(u32 offset) {
    if (base_address != 0)
        return Memory::GetPointer(base_address + offset);

    LOG_ERROR(Kernel_SVC, "memory block id=%u not mapped!", GetObjectId());
    return nullptr;
}

} // namespace
