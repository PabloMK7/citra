// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// DO NOT EVER INCLUDE <windows.h> directly _or indirectly_ from this file
// since it slows down the build a lot.

#include <cstdlib>
#include <cstdio>
#include <cstring>

#define STACKALIGN

// An inheritable class to disallow the copy constructor and operator= functions
class NonCopyable {
protected:
    NonCopyable() = default;
    ~NonCopyable() = default;

    NonCopyable(NonCopyable&) = delete;
    NonCopyable& operator=(NonCopyable&) = delete;
};

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/common_types.h"
#include "common/common_funcs.h"
#include "common/common_paths.h"
#include "common/platform.h"

#ifdef __APPLE__
// The Darwin ABI requires that stack frames be aligned to 16-byte boundaries.
// This is only needed on i386 gcc - x86_64 already aligns to 16 bytes.
    #if defined __i386__ && defined __GNUC__
        #undef STACKALIGN
        #define STACKALIGN __attribute__((__force_align_arg_pointer__))
    #endif
#elif defined _WIN32
// Check MSC ver
    #if defined _MSC_VER && _MSC_VER <= 1000
        #error needs at least version 1000 of MSC
    #endif

    #ifndef NOMINMAX
    #define NOMINMAX
    #endif

// Alignment
    #define MEMORY_ALIGNED16(x) __declspec(align(16)) x
    #define MEMORY_ALIGNED32(x) __declspec(align(32)) x
    #define MEMORY_ALIGNED64(x) __declspec(align(64)) x
    #define MEMORY_ALIGNED128(x) __declspec(align(128)) x
    #define MEMORY_ALIGNED16_DECL(x) __declspec(align(16)) x
    #define MEMORY_ALIGNED64_DECL(x) __declspec(align(64)) x
#endif

// Windows compatibility
#ifndef _WIN32
    #ifdef _LP64
        #define _M_X64 1
    #else
        #define _M_IX86 1
    #endif
    #define __forceinline inline __attribute__((always_inline))
    #define MEMORY_ALIGNED16(x) __attribute__((aligned(16))) x
    #define MEMORY_ALIGNED32(x) __attribute__((aligned(32))) x
    #define MEMORY_ALIGNED64(x) __attribute__((aligned(64))) x
    #define MEMORY_ALIGNED128(x) __attribute__((aligned(128))) x
    #define MEMORY_ALIGNED16_DECL(x) __attribute__((aligned(16))) x
    #define MEMORY_ALIGNED64_DECL(x) __attribute__((aligned(64))) x
#endif

#ifdef _MSC_VER
    #define __strdup _strdup
    #define __getcwd _getcwd
    #define __chdir _chdir
#else
    #define __strdup strdup
    #define __getcwd getcwd
    #define __chdir chdir
#endif

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

#include "swap.h"
