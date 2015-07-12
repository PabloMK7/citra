// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common_types.h"

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/// Textually concatenates two tokens. The double-expansion is required by the C preprocessor.
#define CONCAT2(x, y) DO_CONCAT2(x, y)
#define DO_CONCAT2(x, y) x ## y

// helper macro to properly align structure members.
// Calling INSERT_PADDING_BYTES will add a new member variable with a name like "pad121",
// depending on the current source line to make sure variable names are unique.
#define INSERT_PADDING_BYTES(num_bytes) u8 CONCAT2(pad, __LINE__)[(num_bytes)]
#define INSERT_PADDING_WORDS(num_words) u32 CONCAT2(pad, __LINE__)[(num_words)]

#ifdef _WIN32
    // Alignment
    #define MEMORY_ALIGNED16(x) __declspec(align(16)) x
    #define MEMORY_ALIGNED32(x) __declspec(align(32)) x
    #define MEMORY_ALIGNED64(x) __declspec(align(64)) x
    #define MEMORY_ALIGNED128(x) __declspec(align(128)) x
#else
    #define __forceinline inline __attribute__((always_inline))
    #define MEMORY_ALIGNED16(x) __attribute__((aligned(16))) x
    #define MEMORY_ALIGNED32(x) __attribute__((aligned(32))) x
    #define MEMORY_ALIGNED64(x) __attribute__((aligned(64))) x
    #define MEMORY_ALIGNED128(x) __attribute__((aligned(128))) x
#endif

#ifndef _MSC_VER

#if defined(__x86_64__) || defined(_M_X64)
#define Crash() __asm__ __volatile__("int $3")
#elif defined(_M_ARM)
#define Crash() __asm__ __volatile__("trap")
#else
#define Crash() exit(1)
#endif

// GCC 4.8 defines all the rotate functions now
// Small issue with GCC's lrotl/lrotr intrinsics is they are still 32bit while we require 64bit
#ifndef _rotl
inline u32 _rotl(u32 x, int shift) {
    shift &= 31;
    if (!shift) return x;
    return (x << shift) | (x >> (32 - shift));
}

inline u32 _rotr(u32 x, int shift) {
    shift &= 31;
    if (!shift) return x;
    return (x >> shift) | (x << (32 - shift));
}
#endif

inline u64 _rotl64(u64 x, unsigned int shift){
    unsigned int n = shift % 64;
    return (x << n) | (x >> (64 - n));
}

inline u64 _rotr64(u64 x, unsigned int shift){
    unsigned int n = shift % 64;
    return (x >> n) | (x << (64 - n));
}

#else // _MSC_VER
    #if (_MSC_VER < 1900)
        // Function Cross-Compatibility
        #define snprintf _snprintf
    #endif

    // Locale Cross-Compatibility
    #define locale_t _locale_t

    extern "C" {
        __declspec(dllimport) void __stdcall DebugBreak(void);
    }
    #define Crash() {DebugBreak();}
#endif // _MSC_VER ndef

// Generic function to get last error message.
// Call directly after the command or use the error num.
// This function might change the error code.
// Defined in Misc.cpp.
const char* GetLastErrorMsg();
