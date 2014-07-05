// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"

namespace Kernel {

/**
 * Creates a shared memory object
 * @param name Optional name of shared memory object
 * @return Handle of newly created shared memory object
 */
Handle CreateSharedMemory(const std::string& name="Unknown");

/**
 * Maps a shared memory block to an address in system memory
 * @param handle Shared memory block handle
 * @param address Address in system memory to map shared memory block to
 * @param permissions Memory block map permissions (specified by SVC field)
 * @param other_permissions Memory block map other permissions (specified by SVC field)
 * @return Result of operation, 0 on success, otherwise error code
 */
Result MapSharedMemory(u32 handle, u32 address, u32 permissions, u32 other_permissions);

/**
 * Gets a pointer to the shared memory block
 * @param handle Shared memory block handle
 * @param offset Offset from the start of the shared memory block to get pointer
 * @return Pointer to the shared memory block from the specified offset
 */
u8* GetSharedMemoryPointer(Handle handle, u32 offset);

} // namespace
