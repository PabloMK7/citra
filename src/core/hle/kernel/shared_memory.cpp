// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common.h"

#include "core/mem_map.h"
#include "core/hle/kernel/shared_memory.h"

namespace Kernel {

ResultVal<SharedPtr<SharedMemory>> SharedMemory::Create(std::string name) {
    SharedPtr<SharedMemory> shared_memory(new SharedMemory);

    // TOOD(yuriks): Don't create Handle (see Thread::Create())
    CASCADE_RESULT(auto unused, Kernel::g_handle_table.Create(shared_memory));

    shared_memory->name = std::move(name);
    return MakeResult<SharedPtr<SharedMemory>>(std::move(shared_memory));
}

ResultCode SharedMemory::Map(VAddr address, MemoryPermission permissions,
        MemoryPermission other_permissions) {

    if (address < Memory::SHARED_MEMORY_VADDR || address >= Memory::SHARED_MEMORY_VADDR_END) {
        LOG_ERROR(Kernel, "cannot map handle=0x%08X, address=0x%08X outside of shared mem bounds!",
                GetHandle(), address);
        // TODO: Verify error code with hardware
        return ResultCode(ErrorDescription::InvalidAddress, ErrorModule::Kernel,
                ErrorSummary::InvalidArgument, ErrorLevel::Permanent);
    }

    base_address = address;
    permissions = permissions;
    other_permissions = other_permissions;

    return RESULT_SUCCESS;
}

ResultVal<u8*> SharedMemory::GetPointer(u32 offset) {
    if (base_address != 0)
        return MakeResult<u8*>(Memory::GetPointer(base_address + offset));

    LOG_ERROR(Kernel_SVC, "memory block handle=0x%08X not mapped!", GetHandle());
    // TODO(yuriks): Verify error code.
    return ResultCode(ErrorDescription::InvalidAddress, ErrorModule::Kernel,
            ErrorSummary::InvalidState, ErrorLevel::Permanent);
}

} // namespace
