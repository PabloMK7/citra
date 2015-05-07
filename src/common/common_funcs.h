// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common_types.h"
#include <cstdlib>

#ifdef _WIN32
#define SLEEP(x) Sleep(x)
#else
#include <unistd.h>
#define SLEEP(x) usleep(x*1000)
#endif


#define b2(x)   (   (x) | (   (x) >> 1) )
#define b4(x)   ( b2(x) | ( b2(x) >> 2) )
#define b8(x)   ( b4(x) | ( b4(x) >> 4) )
#define b16(x)  ( b8(x) | ( b8(x) >> 8) )
#define b32(x)  (b16(x) | (b16(x) >>16) )
#define ROUND_UP_POW2(x)    (b32(x - 1) + 1)

#define ARRAY_SIZE(a) (sizeof(a) / sizeof(a[0]))

/// Textually concatenates two tokens. The double-expansion is required by the C preprocessor.
#define CONCAT2(x, y) DO_CONCAT2(x, y)
#define DO_CONCAT2(x, y) x ## y

// helper macro to properly align structure members.
// Calling INSERT_PADDING_BYTES will add a new member variable with a name like "pad121",
// depending on the current source line to make sure variable names are unique.
#define INSERT_PADDING_BYTES(num_bytes) u8 CONCAT2(pad, __LINE__)[(num_bytes)]
#define INSERT_PADDING_WORDS(num_words) u32 CONCAT2(pad, __LINE__)[(num_words)]

#ifndef _MSC_VER

#include <errno.h>

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
    #include <locale.h>

    // Function Cross-Compatibility
    #define snprintf _snprintf

    // Locale Cross-Compatibility
    #define locale_t _locale_t
    #define freelocale _free_locale
    #define newlocale(mask, locale, base) _create_locale(mask, locale)

    #define LC_GLOBAL_LOCALE    ((locale_t)-1)
    #define LC_ALL_MASK            LC_ALL
    #define LC_COLLATE_MASK        LC_COLLATE
    #define LC_CTYPE_MASK          LC_CTYPE
    #define LC_MONETARY_MASK       LC_MONETARY
    #define LC_NUMERIC_MASK        LC_NUMERIC
    #define LC_TIME_MASK           LC_TIME

    inline locale_t uselocale(locale_t new_locale)
    {
        // Retrieve the current per thread locale setting
        bool bIsPerThread = (_configthreadlocale(0) == _ENABLE_PER_THREAD_LOCALE);

        // Retrieve the current thread-specific locale
        locale_t old_locale = bIsPerThread ? _get_current_locale() : LC_GLOBAL_LOCALE;

        if(new_locale == LC_GLOBAL_LOCALE)
        {
            // Restore the global locale
            _configthreadlocale(_DISABLE_PER_THREAD_LOCALE);
        }
        else if(new_locale != nullptr)
        {
            // Configure the thread to set the locale only for this thread
            _configthreadlocale(_ENABLE_PER_THREAD_LOCALE);

            // Set all locale categories
            for(int i = LC_MIN; i <= LC_MAX; i++)
                setlocale(i, new_locale->locinfo->lc_category[i].locale);
        }

        return old_locale;
    }

// 64 bit offsets for windows
    #define fseeko _fseeki64
    #define ftello _ftelli64
    #define atoll _atoi64
    #define stat64 _stat64
    #define fstat64 _fstat64
    #define fileno _fileno

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
