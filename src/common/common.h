// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

// DO NOT EVER INCLUDE <windows.h> directly _or indirectly_ from this file
// since it slows down the build a lot.

#include <cstdlib>
#include <cstdio>
#include <cstring>

#include "common/assert.h"
#include "common/logging/log.h"
#include "common/common_types.h"
#include "common/common_funcs.h"
#include "common/common_paths.h"
#include "common/platform.h"

#ifdef _WIN32
    // Alignment
    #define MEMORY_ALIGNED16(x) __declspec(align(16)) x
    #define MEMORY_ALIGNED32(x) __declspec(align(32)) x
    #define MEMORY_ALIGNED64(x) __declspec(align(64)) x
    #define MEMORY_ALIGNED128(x) __declspec(align(128)) x
#else
    // Windows compatibility
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
#endif

#include "swap.h"
