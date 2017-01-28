// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Common {

void MurmurHash3_128(const void* key, size_t len, u32 seed, void* out);

/**
 * Computes a 64-bit hash over the specified block of data
 * @param data Block of data to compute hash over
 * @param len Length of data (in bytes) to compute hash over
 * @returns 64-bit hash value that was computed over the data block
 */
static inline u64 ComputeHash64(const void* data, size_t len) {
    u64 res[2];
    MurmurHash3_128(data, len, 0, res);
    return res[0];
}

} // namespace Common
