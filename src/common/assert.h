// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"

// TODO (yuriks) allow synchronous logging so we don't need printf
#define ASSERT(_a_) \
    do if (!(_a_)) {\
        fprintf(stderr, "Assertion Failed!\n\n  Line: %d\n  File: %s\n  Time: %s\n", \
                     __LINE__, __FILE__, __TIME__); \
        Crash(); \
    } while (0)

#define ASSERT_MSG(_a_, ...) \
    do if (!(_a_)) {\
        fprintf(stderr, "Assertion Failed!\n\n  Line: %d\n  File: %s\n  Time: %s\n", \
                     __LINE__, __FILE__, __TIME__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
        Crash(); \
    } while (0)

#define UNREACHABLE() ASSERT_MSG(false, "Unreachable code!")

#ifdef _DEBUG
#define DEBUG_ASSERT(_a_) ASSERT(_a_)
#define DEBUG_ASSERT_MSG(_a_, ...) ASSERT_MSG(_a_, __VA_ARGS__)
#else // not debug
#define DEBUG_ASSERT(_a_)
#define DEBUG_ASSERT_MSG(_a_, _desc_, ...)
#endif

#define UNIMPLEMENTED() DEBUG_ASSERT_MSG(false, "Unimplemented code!")
