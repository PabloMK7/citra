// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include "common/common.h"

#include "core/mem_map.h"
#include "core/hle/kernel/shared_memory.h"

namespace Kernel {

class SharedMemory : public Object {
public:
    std::string GetTypeName() const override { return "SharedMemory"; }

    static Kernel::HandleType GetStaticHandleType() {  return Kernel::HandleType::SharedMemory; }
    Kernel::HandleType GetHandleType() const override { return Kernel::HandleType::SharedMemory; }

    /**
     * Wait for kernel object to synchronize
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result WaitSynchronization(bool* wait) override {
        // TODO(bunnei): ImplementMe
        ERROR_LOG(OSHLE, "(UNIMPLEMENTED)");
        return 0;
    }

    u32 base_address;                   ///< Address of shared memory block in RAM
    MemoryPermission permissions;       ///< Permissions of shared memory block (SVC field)
    MemoryPermission other_permissions; ///< Other permissions of shared memory block (SVC field)
    std::string name;                   ///< Name of shared memory object (optional)
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Creates a shared memory object
 * @param handle Handle of newly created shared memory object
 * @param name Name of shared memory object
 * @return Pointer to newly created shared memory object
 */
SharedMemory* CreateSharedMemory(Handle& handle, const std::string& name) {
    SharedMemory* shared_memory = new SharedMemory;
    handle = Kernel::g_object_pool.Create(shared_memory);
    shared_memory->name = name;
    return shared_memory;
}

/**
 * Creates a shared memory object
 * @param name Optional name of shared memory object
 * @return Handle of newly created shared memory object
 */
Handle CreateSharedMemory(const std::string& name) {
    Handle handle;
    CreateSharedMemory(handle, name);
    return handle;
}

/**
 * Maps a shared memory block to an address in system memory
 * @param handle Shared memory block handle
 * @param address Address in system memory to map shared memory block to
 * @param permissions Memory block map permissions (specified by SVC field)
 * @param other_permissions Memory block map other permissions (specified by SVC field)
 * @return Result of operation, 0 on success, otherwise error code
 */
Result MapSharedMemory(u32 handle, u32 address, MemoryPermission permissions, 
    MemoryPermission other_permissions) {

    if (address < Memory::SHARED_MEMORY_VADDR || address >= Memory::SHARED_MEMORY_VADDR_END) {
        ERROR_LOG(KERNEL, "cannot map handle=0x%08X, address=0x%08X outside of shared mem bounds!",
            handle);
        return -1;
    }
    SharedMemory* shared_memory = Kernel::g_object_pool.GetFast<SharedMemory>(handle);
    _assert_msg_(KERNEL, (shared_memory != nullptr), "handle 0x%08X is not valid!", handle);

    shared_memory->base_address = address;
    shared_memory->permissions = permissions;
    shared_memory->other_permissions = other_permissions;

    return 0;
}

/**
 * Gets a pointer to the shared memory block
 * @param handle Shared memory block handle
 * @param offset Offset from the start of the shared memory block to get pointer
 * @return Pointer to the shared memory block from the specified offset
 */
u8* GetSharedMemoryPointer(Handle handle, u32 offset) {
    SharedMemory* shared_memory = Kernel::g_object_pool.GetFast<SharedMemory>(handle);
    _assert_msg_(KERNEL, (shared_memory != nullptr), "handle 0x%08X is not valid!", handle);

    if (0 != shared_memory->base_address)
        return Memory::GetPointer(shared_memory->base_address + offset);

    ERROR_LOG(KERNEL, "memory block handle=0x%08X not mapped!", handle);
    return nullptr;
}

} // namespace
