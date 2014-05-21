// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#pragma once

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"

namespace Kernel {

/**
 * Releases a mutex
 * @param handle Handle to mutex to release
 */
Result ReleaseMutex(Handle handle);

/**
 * Creates a mutex
 * @param handle Reference to handle for the newly created mutex
 * @param initial_locked Specifies if the mutex should be locked initially
 */
Result CreateMutex(Handle& handle, bool initial_locked);

} // namespace
