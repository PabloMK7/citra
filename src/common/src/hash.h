/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    hash.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-12-05
 * @brief   General purpose hash function
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

#ifndef COMMON_HASH_H_
#define COMMON_HASH_H_

#include "types.h"

namespace common {

typedef u64 Hash64;

/**
 * Compute an efficient 64-bit hash (optimized for Intel hardware)
 * @param src Source data buffer to compute hash for
 * @param len Length of data buffer to compute hash for
 * @param samples Number of samples to compute hash for
 * @remark Borrowed from Dolphin Emulator
 */
Hash64 GetHash64(const u8 *src, int len, u32 samples);

} // namespace


#endif // COMMON_HASH_H_