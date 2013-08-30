/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    atomic_win32.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-06-28
 * @brief   Cross-platform atomic operations - Windows/Visual C++
 * @remark  Taken from Dolphin Emulator (http://code.google.com/p/dolphin-emu/)
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

#ifndef COMMON_ATOMIC_WIN32_H_
#define COMMON_ATOMIC_WIN32_H_

#include "types.h"

#include <intrin.h>
#include <Windows.h>

namespace common {

inline void AtomicAdd(volatile u32& target, u32 value) {
    InterlockedExchangeAdd((volatile LONG*)&target, (LONG)value);
}

inline void AtomicAnd(volatile u32& target, u32 value) {
    _InterlockedAnd((volatile LONG*)&target, (LONG)value);
}

inline void AtomicIncrement(volatile u32& target) {
    InterlockedIncrement((volatile LONG*)&target);
}

inline void AtomicDecrement(volatile u32& target) {
    InterlockedDecrement((volatile LONG*)&target);
}

inline u32 AtomicLoad(volatile u32& src) {
    return src;
}

inline u32 AtomicLoadAcquire(volatile u32& src) {
    u32 result = src;
    _ReadBarrier();
    return result;
}

inline void AtomicOr(volatile u32& target, u32 value) {
    _InterlockedOr((volatile LONG*)&target, (LONG)value);
}

inline void AtomicStore(volatile u32& dest, u32 value) {
    dest = value;
}
inline void AtomicStoreRelease(volatile u32& dest, u32 value) {
    _WriteBarrier();
    dest = value;
}

} // namespace

#endif // COMMON_ATOMIC_WIN32_H_