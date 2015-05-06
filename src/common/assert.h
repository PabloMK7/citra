// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdio>
#include <cstdlib>

#include "common/common_funcs.h"

// For asserts we'd like to keep all the junk executed when an assert happens away from the
// important code in the function. One way of doing this is to put all the relevant code inside a
// lambda and force the compiler to not inline it. Unfortunately, MSVC seems to have no syntax to
// specify __declspec on lambda functions, so what we do instead is define a noinline wrapper
// template that calls the lambda. This seems to generate an extra instruction at the call-site
// compared to the ideal implementation (which wouldn't support ASSERT_MSG parameters), but is good
// enough for our purposes.
template <typename Fn>
#if defined(_MSC_VER)
    __declspec(noinline, noreturn)
#elif defined(__GNUC__)
    __attribute__((noinline, noreturn, cold))
#endif
static void assert_noinline_call(const Fn& fn) {
    fn();
    Crash();
    exit(1); // Keeps GCC's mouth shut about this actually returning
}

// TODO (yuriks) allow synchronous logging so we don't need printf
#define ASSERT(_a_) \
    do if (!(_a_)) { assert_noinline_call([] { \
        fprintf(stderr, "Assertion Failed!\n\n  Line: %d\n  File: %s\n  Time: %s\n", \
                     __LINE__, __FILE__, __TIME__); \
    }); } while (0)

#define ASSERT_MSG(_a_, ...) \
    do if (!(_a_)) { assert_noinline_call([&] { \
        fprintf(stderr, "Assertion Failed!\n\n  Line: %d\n  File: %s\n  Time: %s\n", \
                     __LINE__, __FILE__, __TIME__); \
        fprintf(stderr, __VA_ARGS__); \
        fprintf(stderr, "\n"); \
    }); } while (0)

#define UNREACHABLE() ASSERT_MSG(false, "Unreachable code!")

#ifdef _DEBUG
#define DEBUG_ASSERT(_a_) ASSERT(_a_)
#define DEBUG_ASSERT_MSG(_a_, ...) ASSERT_MSG(_a_, __VA_ARGS__)
#else // not debug
#define DEBUG_ASSERT(_a_)
#define DEBUG_ASSERT_MSG(_a_, _desc_, ...)
#endif

#define UNIMPLEMENTED() DEBUG_ASSERT_MSG(false, "Unimplemented code!")
