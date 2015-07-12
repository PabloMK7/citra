// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/result.h"

namespace Kernel {

/// Permissions for mapped shared memory blocks
enum class MemoryPermission : u32 {
    None             = 0,
    Read             = (1u <<  0),
    Write            = (1u <<  1),
    ReadWrite        = (Read | Write),
    Execute          = (1u <<  2),
    ReadExecute      = (Read | Execute),
    WriteExecute     = (Write | Execute),
    ReadWriteExecute = (Read | Write | Execute),
    DontCare         = (1u << 28)
};

class SharedMemory final : public Object {
public:
    /**
     * Creates a shared memory object
     * @param size Size of the memory block. Must be page-aligned.
     * @param permissions Permission restrictions applied to the process which created the block.
     * @param other_permissions Permission restrictions applied to other processes mapping the block.
     * @param name Optional object name, used for debugging purposes.
     */
    static SharedPtr<SharedMemory> Create(u32 size, MemoryPermission permissions,
            MemoryPermission other_permissions, std::string name = "Unknown");

    std::string GetTypeName() const override { return "SharedMemory"; }
    std::string GetName() const override { return name; }

    static const HandleType HANDLE_TYPE = HandleType::SharedMemory;
    HandleType GetHandleType() const override { return HANDLE_TYPE; }

    /**
     * Maps a shared memory block to an address in system memory
     * @param address Address in system memory to map shared memory block to
     * @param permissions Memory block map permissions (specified by SVC field)
     * @param other_permissions Memory block map other permissions (specified by SVC field)
     */
    ResultCode Map(VAddr address, MemoryPermission permissions, MemoryPermission other_permissions);

    /**
    * Gets a pointer to the shared memory block
    * @param offset Offset from the start of the shared memory block to get pointer
    * @return Pointer to the shared memory block from the specified offset
    */
    u8* GetPointer(u32 offset = 0);

    /// Address of shared memory block in the process.
    VAddr base_address;
    /// Size of the memory block. Page-aligned.
    u32 size;
    /// Permission restrictions applied to the process which created the block.
    MemoryPermission permissions;
    /// Permission restrictions applied to other processes mapping the block.
    MemoryPermission other_permissions;
    /// Name of shared memory object.
    std::string name;

private:
    SharedMemory();
    ~SharedMemory() override;
};

} // namespace
