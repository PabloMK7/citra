// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/mem_map.h"
#include "core/hle/kernel/shared_memory.h"

namespace Kernel {

SharedMemory::SharedMemory() {}
SharedMemory::~SharedMemory() {}

SharedPtr<SharedMemory> SharedMemory::Create(std::string name) {
    SharedPtr<SharedMemory> shared_memory(new SharedMemory);

    shared_memory->name = std::move(name);
    shared_memory->base_address = 0x0;

    return shared_memory;
}

ResultCode SharedMemory::Map(VAddr address, MemoryPermission permissions,
        MemoryPermission other_permissions) {

    if (address < Memory::SHARED_MEMORY_VADDR || address >= Memory::SHARED_MEMORY_VADDR_END) {
        LOG_ERROR(Kernel, "cannot map id=%u, address=0x%08X outside of shared mem bounds!",
                GetObjectId(), address);
        // TODO: Verify error code with hardware
        return ResultCode(ErrorDescription::InvalidAddress, ErrorModule::Kernel,
                ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
    }

    this->base_address = address;
    this->permissions = permissions;
    this->other_permissions = other_permissions;

    return RESULT_SUCCESS;
}

ResultVal<u8*> SharedMemory::GetPointer(u32 offset) {
    if (base_address != 0)
        return MakeResult<u8*>(Memory::GetPointer(base_address + offset));

    LOG_ERROR(Kernel_SVC, "memory block id=%u not mapped!", GetObjectId());
    // TODO(yuriks): Verify error code.
    return ResultCode(ErrorDescription::InvalidAddress, ErrorModule::Kernel,
            ErrorSummary::InvalidState, ErrorLevel::Permanent);
}

} // namespace
