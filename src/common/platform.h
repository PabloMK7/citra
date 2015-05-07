/**
 * Copyright (C) 2005-2012 Gekko Emulator
 *
 * @file    platform.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-02-11
 * @brief   Platform detection macros for portable compilation
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

#include "common/common_types.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Platform definitions

/// Enumeration for defining the supported platforms
#define PLATFORM_NULL 0
#define PLATFORM_WINDOWS 1
#define PLATFORM_MACOSX 2
#define PLATFORM_LINUX 3
#define PLATFORM_ANDROID 4

////////////////////////////////////////////////////////////////////////////////////////////////////
// Platform detection

#ifndef EMU_PLATFORM

#if defined( __WIN32__ ) || defined( _WIN32 )
#define EMU_PLATFORM PLATFORM_WINDOWS

#elif defined( __APPLE__ ) || defined( __APPLE_CC__ )
#define EMU_PLATFORM PLATFORM_MACOSX

#elif defined(__linux__)
#define EMU_PLATFORM PLATFORM_LINUX

#else // Assume linux otherwise
#define EMU_PLATFORM PLATFORM_LINUX

#endif

#endif

#if defined(__x86_64__) || defined(_M_X64) || defined(__aarch64__)
    #define EMU_ARCH_BITS 64
#elif defined(__i386) || defined(_M_IX86) || defined(__arm__) || defined(_M_ARM)
    #define EMU_ARCH_BITS 32
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// Feature detection

#if defined _M_GENERIC
#  define _M_SSE 0x0
#elif defined __GNUC__
# if defined __SSE4_2__
#  define _M_SSE 0x402
# elif defined __SSE4_1__
#  define _M_SSE 0x401
# elif defined __SSSE3__
#  define _M_SSE 0x301
# elif defined __SSE3__
#  define _M_SSE 0x300
# endif
#elif (_MSC_VER >= 1500) || __INTEL_COMPILER // Visual Studio 2008
#  define _M_SSE 0x402
#endif

////////////////////////////////////////////////////////////////////////////////////////////////////
// Compiler-Specific Definitions

#define GCC_VERSION_AVAILABLE(major, minor) (defined(__GNUC__) &&  (__GNUC__ > (major) || \
    (__GNUC__ == (major) && __GNUC_MINOR__ >= (minor))))
