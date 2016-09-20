// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cstdio>

#ifdef _WIN32
#define WIN32_LEAN_AND_MEAN
#include <Windows.h>
#endif

#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/logging/text_formatter.h"
#include "common/string_util.h"

namespace Log {

// TODO(bunnei): This should be moved to a generic path manipulation library
const char* TrimSourcePath(const char* path, const char* root) {
    const char* p = path;

    while (*p != '\0') {
        const char* next_slash = p;
        while (*next_slash != '\0' && *next_slash != '/' && *next_slash != '\\') {
            ++next_slash;
        }

        bool is_src = Common::ComparePartialString(p, next_slash, root);
        p = next_slash;

        if (*p != '\0') {
            ++p;
        }
        if (is_src) {
            path = p;
        }
    }
    return path;
}

void FormatLogMessage(const Entry& entry, char* out_text, size_t text_len) {
    unsigned int time_seconds = static_cast<unsigned int>(entry.timestamp.count() / 1000000);
    unsigned int time_fractional = static_cast<unsigned int>(entry.timestamp.count() % 1000000);

    const char* class_name = GetLogClassName(entry.log_class);
    const char* level_name = GetLevelName(entry.log_level);

    snprintf(out_text, text_len, "[%4u.%06u] %s <%s> %s: %s", time_seconds, time_fractional,
             class_name, level_name, TrimSourcePath(entry.location.c_str()), entry.message.c_str());
}

void PrintMessage(const Entry& entry) {
    std::array<char, 4 * 1024> format_buffer;
    FormatLogMessage(entry, format_buffer.data(), format_buffer.size());
    fputs(format_buffer.data(), stderr);
    fputc('\n', stderr);
}

void PrintColoredMessage(const Entry& entry) {
#ifdef _WIN32
    static HANDLE console_handle = GetStdHandle(STD_ERROR_HANDLE);

    CONSOLE_SCREEN_BUFFER_INFO original_info = {0};
    GetConsoleScreenBufferInfo(console_handle, &original_info);

    WORD color = 0;
    switch (entry.log_level) {
    case Level::Trace: // Grey
        color = FOREGROUND_INTENSITY;
        break;
    case Level::Debug: // Cyan
        color = FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    case Level::Info: // Bright gray
        color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE;
        break;
    case Level::Warning: // Bright yellow
        color = FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_INTENSITY;
        break;
    case Level::Error: // Bright red
        color = FOREGROUND_RED | FOREGROUND_INTENSITY;
        break;
    case Level::Critical: // Bright magenta
        color = FOREGROUND_RED | FOREGROUND_BLUE | FOREGROUND_INTENSITY;
        break;
    case Level::Count:
        UNREACHABLE();
    }

    SetConsoleTextAttribute(console_handle, color);
#else
#define ESC "\x1b"
    const char* color = "";
    switch (entry.log_level) {
    case Level::Trace: // Grey
        color = ESC "[1;30m";
        break;
    case Level::Debug: // Cyan
        color = ESC "[0;36m";
        break;
    case Level::Info: // Bright gray
        color = ESC "[0;37m";
        break;
    case Level::Warning: // Bright yellow
        color = ESC "[1;33m";
        break;
    case Level::Error: // Bright red
        color = ESC "[1;31m";
        break;
    case Level::Critical: // Bright magenta
        color = ESC "[1;35m";
        break;
    case Level::Count:
        UNREACHABLE();
    }

    fputs(color, stderr);
#endif

    PrintMessage(entry);

#ifdef _WIN32
    SetConsoleTextAttribute(console_handle, original_info.wAttributes);
#else
    fputs(ESC "[0m", stderr);
#undef ESC
#endif
}
}
