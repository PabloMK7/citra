// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include "common/cityhash.h"
#include "common/common_types.h"

namespace Common {

/**
 * Computes a 64-bit hash over the specified block of data
 * @param data Block of data to compute hash over
 * @param len Length of data (in bytes) to compute hash over
 * @returns 64-bit hash value that was computed over the data block
 */
static inline u64 ComputeHash64(const void* data, size_t len) {
    return CityHash64(static_cast<const char*>(data), len);
}

} // namespace Common
