// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"

namespace Kernel {

/**
 * Creates a semaphore
 * @param initial_count number of reserved entries in the semaphore
 * @param max_count maximum number of holders the semaphore can have
  * @param name Optional name of semaphore
 * @return Handle to newly created object
 */
Handle CreateSemaphore(u32 initial_count, u32 max_count, const std::string& name = "Unknown");

} // namespace
