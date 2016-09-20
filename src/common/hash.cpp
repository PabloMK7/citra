// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#if defined(_MSC_VER)
#include <stdlib.h>
#endif
#include "common_funcs.h"
#include "common_types.h"
#include "hash.h"

namespace Common {

// MurmurHash3 was written by Austin Appleby, and is placed in the public
// domain. The author hereby disclaims copyright to this source code.

// Block read - if your platform needs to do endian-swapping or can only handle aligned reads, do
// the conversion here
static FORCE_INLINE u64 getblock64(const u64* p, int i) {
    return p[i];
}

// Finalization mix - force all bits of a hash block to avalanche
static FORCE_INLINE u64 fmix64(u64 k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdllu;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53llu;
    k ^= k >> 33;

    return k;
}

// This is the 128-bit variant of the MurmurHash3 hash function that is targetted for 64-bit
// platforms (MurmurHash3_x64_128). It was taken from:
// https://code.google.com/p/smhasher/source/browse/trunk/MurmurHash3.cpp
void MurmurHash3_128(const void* key, int len, u32 seed, void* out) {
    const u8* data = (const u8*)key;
    const int nblocks = len / 16;

    u64 h1 = seed;
    u64 h2 = seed;

    const u64 c1 = 0x87c37b91114253d5llu;
    const u64 c2 = 0x4cf5ad432745937fllu;

    // Body

    const u64* blocks = (const u64*)(data);

    for (int i = 0; i < nblocks; i++) {
        u64 k1 = getblock64(blocks, i * 2 + 0);
        u64 k2 = getblock64(blocks, i * 2 + 1);

        k1 *= c1;
        k1 = _rotl64(k1, 31);
        k1 *= c2;
        h1 ^= k1;

        h1 = _rotl64(h1, 27);
        h1 += h2;
        h1 = h1 * 5 + 0x52dce729;

        k2 *= c2;
        k2 = _rotl64(k2, 33);
        k2 *= c1;
        h2 ^= k2;

        h2 = _rotl64(h2, 31);
        h2 += h1;
        h2 = h2 * 5 + 0x38495ab5;
    }

    // Tail

    const u8* tail = (const u8*)(data + nblocks * 16);

    u64 k1 = 0;
    u64 k2 = 0;

    switch (len & 15) {
    case 15:
        k2 ^= ((u64)tail[14]) << 48;
    case 14:
        k2 ^= ((u64)tail[13]) << 40;
    case 13:
        k2 ^= ((u64)tail[12]) << 32;
    case 12:
        k2 ^= ((u64)tail[11]) << 24;
    case 11:
        k2 ^= ((u64)tail[10]) << 16;
    case 10:
        k2 ^= ((u64)tail[9]) << 8;
    case 9:
        k2 ^= ((u64)tail[8]) << 0;
        k2 *= c2;
        k2 = _rotl64(k2, 33);
        k2 *= c1;
        h2 ^= k2;

    case 8:
        k1 ^= ((u64)tail[7]) << 56;
    case 7:
        k1 ^= ((u64)tail[6]) << 48;
    case 6:
        k1 ^= ((u64)tail[5]) << 40;
    case 5:
        k1 ^= ((u64)tail[4]) << 32;
    case 4:
        k1 ^= ((u64)tail[3]) << 24;
    case 3:
        k1 ^= ((u64)tail[2]) << 16;
    case 2:
        k1 ^= ((u64)tail[1]) << 8;
    case 1:
        k1 ^= ((u64)tail[0]) << 0;
        k1 *= c1;
        k1 = _rotl64(k1, 31);
        k1 *= c2;
        h1 ^= k1;
    };

    // Finalization

    h1 ^= len;
    h2 ^= len;

    h1 += h2;
    h2 += h1;

    h1 = fmix64(h1);
    h2 = fmix64(h2);

    h1 += h2;
    h2 += h1;

    ((u64*)out)[0] = h1;
    ((u64*)out)[1] = h2;
}

} // namespace Common
