// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"
#include "core/hle/kernel/process.h"
#include "core/hle/result.h"

namespace Kernel {

/// Permissions for mapped shared memory blocks
enum class MemoryPermission : u32 {
    None = 0,
    Read = (1u << 0),
    Write = (1u << 1),
    ReadWrite = (Read | Write),
    Execute = (1u << 2),
    ReadExecute = (Read | Execute),
    WriteExecute = (Write | Execute),
    ReadWriteExecute = (Read | Write | Execute),
    DontCare = (1u << 28)
};

class SharedMemory final : public Object {
public:
    /**
     * Creates a shared memory object.
     * @param owner_process Process that created this shared memory object.
     * @param size Size of the memory block. Must be page-aligned.
     * @param permissions Permission restrictions applied to the process which created the block.
     * @param other_permissions Permission restrictions applied to other processes mapping the
     * block.
     * @param address The address from which to map the Shared Memory.
     * @param region If the address is 0, the shared memory will be allocated in this region of the
     * linear heap.
     * @param name Optional object name, used for debugging purposes.
     */
    static SharedPtr<SharedMemory> Create(SharedPtr<Process> owner_process, u32 size,
                                          MemoryPermission permissions,
                                          MemoryPermission other_permissions, VAddr address = 0,
                                          MemoryRegion region = MemoryRegion::BASE,
                                          std::string name = "Unknown");

    /**
     * Creates a shared memory object from a block of memory managed by an HLE applet.
     * @param heap_block Heap block of the HLE applet.
     * @param offset The offset into the heap block that the SharedMemory will map.
     * @param size Size of the memory block. Must be page-aligned.
     * @param permissions Permission restrictions applied to the process which created the block.
     * @param other_permissions Permission restrictions applied to other processes mapping the
     * block.
     * @param name Optional object name, used for debugging purposes.
     */
    static SharedPtr<SharedMemory> CreateForApplet(std::shared_ptr<std::vector<u8>> heap_block,
                                                   u32 offset, u32 size,
                                                   MemoryPermission permissions,
                                                   MemoryPermission other_permissions,
                                                   std::string name = "Unknown Applet");

    std::string GetTypeName() const override {
        return "SharedMemory";
    }
    std::string GetName() const override {
        return name;
    }

    static const HandleType HANDLE_TYPE = HandleType::SharedMemory;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    /**
     * Converts the specified MemoryPermission into the equivalent VMAPermission.
     * @param permission The MemoryPermission to convert.
     */
    static VMAPermission ConvertPermissions(MemoryPermission permission);

    /**
     * Maps a shared memory block to an address in the target process' address space
     * @param target_process Process on which to map the memory block.
     * @param address Address in system memory to map shared memory block to
     * @param permissions Memory block map permissions (specified by SVC field)
     * @param other_permissions Memory block map other permissions (specified by SVC field)
     */
    ResultCode Map(Process* target_process, VAddr address, MemoryPermission permissions,
                   MemoryPermission other_permissions);

    /**
     * Unmaps a shared memory block from the specified address in system memory
     * @param target_process Process from which to umap the memory block.
     * @param address Address in system memory where the shared memory block is mapped
     * @return Result code of the unmap operation
     */
    ResultCode Unmap(Process* target_process, VAddr address);

    /**
     * Gets a pointer to the shared memory block
     * @param offset Offset from the start of the shared memory block to get pointer
     * @return Pointer to the shared memory block from the specified offset
     */
    u8* GetPointer(u32 offset = 0);

    /// Process that created this shared memory block.
    SharedPtr<Process> owner_process;
    /// Address of shared memory block in the owner process if specified.
    VAddr base_address;
    /// Physical address of the shared memory block in the linear heap if no address was specified
    /// during creation.
    PAddr linear_heap_phys_address;
    /// Backing memory for this shared memory block.
    std::shared_ptr<std::vector<u8>> backing_block;
    /// Offset into the backing block for this shared memory.
    std::size_t backing_block_offset;
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

} // namespace Kernel
