// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"

namespace Kernel {

/**
 * Releases a mutex
 * @param handle Handle to mutex to release
 */
ResultCode ReleaseMutex(Handle handle);

/**
 * Creates a mutex
 * @param initial_locked Specifies if the mutex should be locked initially
 * @param name Optional name of mutex
 * @return Handle to newly created object
 */
Handle CreateMutex(bool initial_locked, const std::string& name="Unknown");

/**
 * Releases all the mutexes held by the specified thread
 * @param thread Thread that is holding the mutexes
 */
void ReleaseThreadMutexes(Thread* thread);

} // namespace
