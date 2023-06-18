// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#ifdef _WIN32
#include <windows.h>
#else
#include <cerrno>
#include <cstring>
#endif

#include "common/common_funcs.h"

// Generic function to get last error message.
// Call directly after the command or use the error num.
// This function might change the error code.
std::string GetLastErrorMsg() {
#ifdef _WIN32
    LPSTR err_str;

    DWORD res = FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM | FORMAT_MESSAGE_ALLOCATE_BUFFER |
                                   FORMAT_MESSAGE_IGNORE_INSERTS,
                               nullptr, GetLastError(), MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT),
                               reinterpret_cast<LPSTR>(&err_str), 1, nullptr);
    if (!res) {
        return "(FormatMessageA failed to format error)";
    }
    std::string ret(err_str);
    LocalFree(err_str);
    return ret;
#else
    char err_str[255];
#if (defined(__GLIBC__) || __ANDROID_API__ >= 23) &&                                               \
    (_GNU_SOURCE || (_POSIX_C_SOURCE < 200112L && _XOPEN_SOURCE < 600))
    // Thread safe (GNU-specific)
    const char* str = strerror_r(errno, err_str, sizeof(err_str));
    return std::string(str);
#else
    // Thread safe (XSI-compliant)
    int second_err = strerror_r(errno, err_str, sizeof(err_str));
    if (second_err != 0) {
        return "(strerror_r failed to format error)";
    }
    return std::string(err_str);
#endif // GLIBC etc.
#endif // _WIN32
}
