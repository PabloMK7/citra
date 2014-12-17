// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"

namespace Kernel {

/**
 * Creates a semaphore.
 * @param handle Pointer to the handle of the newly created object
 * @param initial_count Number of slots reserved for other threads
 * @param max_count Maximum number of slots the semaphore can have
 * @param name Optional name of semaphore
 * @return ResultCode of the error
 */
ResultCode CreateSemaphore(Handle* handle, u32 initial_count, u32 max_count, const std::string& name = "Unknown");

/**
 * Releases a certain number of slots from a semaphore.
 * @param count The number of free slots the semaphore had before this call
 * @param handle The handle of the semaphore to release
 * @param release_count The number of slots to release
 * @return ResultCode of the error
 */
ResultCode ReleaseSemaphore(s32* count, Handle handle, s32 release_count);

} // namespace
