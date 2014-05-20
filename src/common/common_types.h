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

#include <math.h>
#include <xmmintrin.h> // data_types__m128.cpp

#ifdef _WIN32

#include <tchar.h>

typedef unsigned __int8     u8;     ///< 8-bit unsigned byte
typedef unsigned __int16    u16;    ///< 16-bit unsigned short
typedef unsigned __int32    u32;    ///< 32-bit unsigned word
typedef unsigned __int64    u64;    ///< 64-bit unsigned int

typedef signed __int8       s8;     ///< 8-bit signed byte
typedef signed __int16      s16;    ///< 16-bit signed short
typedef signed __int32      s32;    ///< 32-bit signed word
typedef signed __int64      s64;    ///< 64-bit signed int

#else

typedef unsigned char       u8;     ///< 8-bit unsigned byte
typedef unsigned short      u16;    ///< 16-bit unsigned short
typedef unsigned int        u32;    ///< 32-bit unsigned word
typedef unsigned long long  u64;    ///< 64-bit unsigned int

typedef signed char         s8;     ///< 8-bit signed byte
typedef signed short        s16;    ///< 16-bit signed short
typedef signed int          s32;    ///< 32-bit signed word
typedef signed long long    s64;    ///< 64-bit signed int

// For using windows lock code
#define TCHAR char
#define LONG int

#endif // _WIN32

typedef float   f32;    ///< 32-bit floating point
typedef double  f64;    ///< 64-bit floating point

#include "common/common.h"

/// Union for fast 16-bit type casting
union t16 {
	u8	_u8[2];             ///< 8-bit unsigned char(s)
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

/// Union for fast 128-bit type casting
union t128 {
    struct
    {
        t64 ps0;            ///< 64-bit paired single 0
        t64 ps1;            ///< 64-bit paired single 1
    };
    __m128  a;              ///< 128-bit floating point (__m128 maps to the XMM[0-7] registers)
};

namespace common {
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

    inline u32 width() const { return abs(x1_ - x0_); }
    inline u32 height() const { return abs(y1_ - y0_); }

    inline bool operator == (const Rect& val) const {
        return (x0_ == val.x0_ && y0_ == val.y0_ && x1_ == val.x1_ && y1_ == val.y1_);
    }
};
}
