/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    atomic_gcc.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-06-28
 * @brief   Cross-platform atomic operations - GCC
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

#ifndef COMMON_ATOMIC_GCC_H_
#define COMMON_ATOMIC_GCC_H_

#include "types.h"

namespace common {

inline void AtomicAdd(volatile u32& target, u32 value) {
    __sync_add_and_fetch(&target, value);
}

inline void AtomicAnd(volatile u32& target, u32 value) {
    __sync_and_and_fetch(&target, value);
}

inline void AtomicDecrement(volatile u32& target) {
    __sync_add_and_fetch(&target, -1);
}

inline void AtomicIncrement(volatile u32& target) {
    __sync_add_and_fetch(&target, 1);
}

inline u32 AtomicLoad(volatile u32& src) {
    return src;
}

inline u32 AtomicLoadAcquire(volatile u32& src) {
    u32 result = src;
    __asm__ __volatile__ ( "":::"memory" );
    return result;
}

inline void AtomicOr(volatile u32& target, u32 value) {
    __sync_or_and_fetch(&target, value);
}

inline void AtomicStore(volatile u32& dest, u32 value) {
    dest = value;
}

inline void AtomicStoreRelease(volatile u32& dest, u32 value) {
    __sync_lock_test_and_set(&dest, value);
}

} // namespace

#endif // COMMON_ATOMIC_GCC_H_