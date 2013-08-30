/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    hash.cpp
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-12-05
 * @brief   General purpose hash function
 * @remark  Some functions borrowed from Dolphin Emulator
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include "crc.h"
#include "hash.h"
#include "common.h"

namespace common {

/// Block mix - combine the key bits with the hash bits and scramble everything
inline void bmix64(u64& h1, u64& h2, u64& k1, u64& k2, u64& c1, u64& c2) {
    k1 *= c1; 
    k1  = _rotl64(k1,23); 
    k1 *= c2;
    h1 ^= k1;
    h1 += h2;

    h2 = _rotl64(h2,41);

    k2 *= c2; 
    k2  = _rotl64(k2,23);
    k2 *= c1;
    h2 ^= k2;
    h2 += h1;

    h1 = h1*3 + 0x52dce729;
    h2 = h2*3 + 0x38495ab5;

    c1 = c1*5 + 0x7b7d159c;
    c2 = c2*5 + 0x6bce6396;
}

/// Finalization mix - avalanches all bits to within 0.05% bias
inline u64 fmix64(u64 k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccd;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53;
    k ^= k >> 33;
    return k;
}

#define ROTL32(x,y)     rotl32(x,y)

inline uint32_t fmix ( uint32_t h )
{
  h ^= h >> 16;
  h *= 0x85ebca6b;
  h ^= h >> 13;
  h *= 0xc2b2ae35;
  h ^= h >> 16;

  return h;
}

u32 __compute_murmur_hash3_32(const u8 *src, int len, u32 samples) {
    u32 h = len;
    u32 step = (len >> 2);
    const u32 *data = (const u32*)src;
    const u32 *end = data + step;
    if (samples == 0) {
        samples = std::max(step, 1u);
    }
    step  = step / samples;
    if(step < 1) { 
        step = 1;
    }
    u32 h1 = 0x2f6af274;
    const u32 c1 = 0xcc9e2d51;
    const u32 c2 = 0x1b873593;

    while (data < end) {
        u32 k1 = data[0];

        k1 *= c1;
        k1 = (k1 << 15) | (k1 >> (32 - 15));
        k1 *= c2;

        h1 ^= k1;
        h1 = (h1 << 15) | (h1 >> (32 - 13)); 
        h1 = h1*5+0xe6546b64;

        data += step;
    }
    const u8 * tail = (const u8*)(data);

    u32 k1 = 0;

    switch(len & 3) {
    case 3: 
        k1 ^= tail[2] << 16;
    case 2: 
        k1 ^= tail[1] << 8;
    case 1: 
        k1 ^= tail[0];
        k1 *= c1; 
        k1 = (k1 << 15) | (k1 >> (32 - 15));
        k1 *= c2; 
        h1 ^= k1;
    };
    h1 ^= len;
    h1 = fmix(h1);

    return h1;
} 


/// MurmurHash is a non-cryptographic hash function suitable for general hash-based lookup
u64 __compute_murmur_hash3_64(const u8 *src, int len, u32 samples) {
    const u8 * data = (const u8*)src;
    const int nblocks = len / 16;
    u32 step = (len / 8);
    if(samples == 0) {
        samples = std::max(step, 1u);
    }
    step = step / samples;
    if(step < 1) {
        step = 1;
    }

    u64 h1 = 0x9368e53c2f6af274;
    u64 h2 = 0x586dcd208f7cd3fd;

    u64 c1 = 0x87c37b91114253d5;
    u64 c2 = 0x4cf5ad432745937f;

    const u64* blocks = (const u64*)(data);

    for (int i = 0; i < nblocks; i+=step) {
        u64 k1 = blocks[(i * 2) + 0];
        u64 k2 = blocks[(i * 2) + 1];

        bmix64(h1,h2,k1,k2,c1,c2);
    }
    const u8* tail = (const u8*)(data + nblocks * 16);

    u64 k1 = 0;
    u64 k2 = 0;

    switch (len & 15) {
    case 15: k2 ^= u64(tail[14]) << 48;
    case 14: k2 ^= u64(tail[13]) << 40;
    case 13: k2 ^= u64(tail[12]) << 32;
    case 12: k2 ^= u64(tail[11]) << 24;
    case 11: k2 ^= u64(tail[10]) << 16;
    case 10: k2 ^= u64(tail[ 9]) << 8;
    case  9: k2 ^= u64(tail[ 8]) << 0;

    case  8: k1 ^= u64(tail[ 7]) << 56;
    case  7: k1 ^= u64(tail[ 6]) << 48;
    case  6: k1 ^= u64(tail[ 5]) << 40;
    case  5: k1 ^= u64(tail[ 4]) << 32;
    case  4: k1 ^= u64(tail[ 3]) << 24;
    case  3: k1 ^= u64(tail[ 2]) << 16;
    case  2: k1 ^= u64(tail[ 1]) << 8;
    case  1: k1 ^= u64(tail[ 0]) << 0;
             bmix64(h1, h2, k1, k2, c1, c2);
    };
    h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 += h2;

    return h1;
}

/// CRC32 hash using the SSE4.2 instruction
u64 __compute_crc32_sse4(const u8 *src, int len, u32 samples) {
    u32 h = len;
    u32 step = (len >> 2);
    const u32 *data = (const u32*)src;
    const u32 *end = data + step;
    if (samples == 0) {
        samples = std::max(step, 1u);
    }
    step  = step / samples;
    if(step < 1) { 
        step = 1;
    }
    while (data < end) {
        h = InlineCrc32_U32(h, data[0]);
        data += step;
    }
    const u8 *data2 = (const u8*)end;
    return (u64)InlineCrc32_U32(h, u32(data2[0]));
}

/**
 * Compute an efficient 64-bit hash (optimized for Intel hardware)
 * @param src Source data buffer to compute hash for
 * @param len Length of data buffer to compute hash for
 * @param samples Number of samples to compute hash for
 * @remark Borrowed from Dolphin Emulator
 */
Hash64 GetHash64(const u8 *src, int len, u32 samples) {
#if defined(EMU_ARCHITECTURE_X86) || defined(EMU_ARCHITECTURE_X64)
    // TODO(ShizZy): Move somewhere common so we dont need to instantiate this more than once
    static X86Utils x86_utils; 
    if (x86_utils.IsExtensionSupported(X86Utils::kExtensionX86_SSE4_2)) {
        return __compute_crc32_sse4(src, len, samples);
    } else {

#ifdef EMU_ARCHITECTURE_X64
        return __compute_murmur_hash3_64(src, len, samples);
#else
        return __compute_murmur_hash3_32(src, len, samples);
#endif
    }
#else
    return __compute_murmur_hash3_32(src, len, samples);
#endif
}

} // namespace
