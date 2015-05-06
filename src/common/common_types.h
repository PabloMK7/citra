/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    common_types.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-02-11
 * @brief   Common types used throughout the project
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

#pragma once

#include <cmath>
#include <cstdint>
#include <cstdlib>

#ifdef _MSC_VER
#ifndef __func__
#define __func__ __FUNCTION__
#endif
#endif

typedef std::uint8_t  u8;  ///< 8-bit unsigned byte
typedef std::uint16_t u16; ///< 16-bit unsigned short
typedef std::uint32_t u32; ///< 32-bit unsigned word
typedef std::uint64_t u64; ///< 64-bit unsigned int

typedef std::int8_t  s8;  ///< 8-bit signed byte
typedef std::int16_t s16; ///< 16-bit signed short
typedef std::int32_t s32; ///< 32-bit signed word
typedef std::int64_t s64; ///< 64-bit signed int

typedef float   f32; ///< 32-bit floating point
typedef double  f64; ///< 64-bit floating point

// TODO: It would be nice to eventually replace these with strong types that prevent accidental
// conversion between each other.
typedef u32 VAddr; ///< Represents a pointer in the userspace virtual address space.
typedef u32 PAddr; ///< Represents a pointer in the ARM11 physical address space.

/// Union for fast 16-bit type casting
union t16 {
    u8  _u8[2];             ///< 8-bit unsigned char(s)
    u16 _u16;               ///< 16-bit unsigned shorts(s)
};

/// Union for fast 32-bit type casting
union t32 {
    f32 _f32;               ///< 32-bit floating point(s)
    u32 _u32;               ///< 32-bit unsigned int(s)
    s32 _s32;               ///< 32-bit signed int(s)
    u16 _u16[2];            ///< 16-bit unsigned shorts(s)
    u8  _u8[4];             ///< 8-bit unsigned char(s)
};

/// Union for fast 64-bit type casting
union t64 {
    f64 _f64;               ///< 64-bit floating point
    u64 _u64;               ///< 64-bit unsigned long
    f32 _f32[2];            ///< 32-bit floating point(s)
    u32 _u32[2];            ///< 32-bit unsigned int(s)
    s32 _s32[2];            ///< 32-bit signed int(s)
    u16 _u16[4];            ///< 16-bit unsigned shorts(s)
    u8  _u8[8];             ///< 8-bit unsigned char(s)
};

// An inheritable class to disallow the copy constructor and operator= functions
class NonCopyable {
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;

    NonCopyable(NonCopyable&) = delete;
    NonCopyable& operator=(NonCopyable&) = delete;
};

namespace Common {
/// Rectangle data structure
class Rect {
public:
    Rect(int x0=0, int y0=0, int x1=0, int y1=0) {
        x0_ = x0;
        y0_ = y0;
        x1_ = x1;
        y1_ = y1;
    }
    ~Rect() { }

    int x0_;    ///< Rect top left X-coordinate
    int y0_;    ///< Rect top left Y-coordinate
    int x1_;    ///< Rect bottom left X-coordinate
    int y1_;    ///< Rect bottom right Y-coordinate

    inline u32 width() const { return std::abs(x1_ - x0_); }
    inline u32 height() const { return std::abs(y1_ - y0_); }

    inline bool operator == (const Rect& val) const {
        return (x0_ == val.x0_ && y0_ == val.y0_ && x1_ == val.x1_ && y1_ == val.y1_);
    }
};
}
