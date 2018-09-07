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
    static const std::size_t buff_size = 255;
    char err_str[buff_size];

#ifdef _WIN32
    FormatMessageA(FORMAT_MESSAGE_FROM_SYSTEM, nullptr, GetLastError(),
                   MAKELANGID(LANG_NEUTRAL, SUBLANG_DEFAULT), err_str, buff_size, nullptr);
#else
    // Thread safe (XSI-compliant)
    strerror_r(errno, err_str, buff_size);
#endif

    return std::string(err_str, buff_size);
}
