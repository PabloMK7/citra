// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstdarg>
#include <cstddef>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>
#include "common/common_types.h"

namespace Common {

/// Make a string lowercase
std::string ToLower(std::string str);

/// Make a string uppercase
std::string ToUpper(std::string str);

std::string StringFromFormat(const char* format, ...);
// Cheap!
bool CharArrayFromFormatV(char* out, int outsize, const char* format, va_list args);

template <size_t Count>
inline void CharArrayFromFormat(char (&out)[Count], const char* format, ...) {
    va_list args;
    va_start(args, format);
    CharArrayFromFormatV(out, Count, format, args);
    va_end(args);
}

// Good
std::string ArrayToString(const u8* data, u32 size, int line_len = 20, bool spaces = true);

std::string StripSpaces(const std::string& s);
std::string StripQuotes(const std::string& s);

// Thousand separator. Turns 12345678 into 12,345,678
template <typename I>
std::string ThousandSeparate(I value, int spaces = 0) {
    std::ostringstream oss;

// std::locale("") seems to be broken on many platforms
#if defined _WIN32 || (defined __linux__ && !defined __clang__)
    oss.imbue(std::locale(""));
#endif
    oss << std::setw(spaces) << value;

    return oss.str();
}

std::string StringFromBool(bool value);

bool TryParse(const std::string& str, bool* output);
bool TryParse(const std::string& str, u32* output);

template <typename N>
static bool TryParse(const std::string& str, N* const output) {
    std::istringstream iss(str);

    N tmp = 0;
    if (iss >> tmp) {
        *output = tmp;
        return true;
    } else
        return false;
}

// TODO: kill this
bool AsciiToHex(const char* _szValue, u32& result);

std::string TabsToSpaces(int tab_size, const std::string& in);

void SplitString(const std::string& str, char delim, std::vector<std::string>& output);

// "C:/Windows/winhelp.exe" to "C:/Windows/", "winhelp", ".exe"
bool SplitPath(const std::string& full_path, std::string* _pPath, std::string* _pFilename,
               std::string* _pExtension);

void BuildCompleteFilename(std::string& _CompleteFilename, const std::string& _Path,
                           const std::string& _Filename);
std::string ReplaceAll(std::string result, const std::string& src, const std::string& dest);

std::string UTF16ToUTF8(const std::u16string& input);
std::u16string UTF8ToUTF16(const std::string& input);

std::string CP1252ToUTF8(const std::string& str);
std::string SHIFTJISToUTF8(const std::string& str);

#ifdef _WIN32
std::string UTF16ToUTF8(const std::wstring& input);
std::wstring UTF8ToUTF16W(const std::string& str);

#ifdef _UNICODE
inline std::string TStrToUTF8(const std::wstring& str) {
    return UTF16ToUTF8(str);
}

inline std::wstring UTF8ToTStr(const std::string& str) {
    return UTF8ToUTF16W(str);
}
#else
inline std::string TStrToUTF8(const std::string& str) {
    return str;
}

inline std::string UTF8ToTStr(const std::string& str) {
    return str;
}
#endif

#endif

/**
 * Compares the string defined by the range [`begin`, `end`) to the null-terminated C-string
 * `other` for equality.
 */
template <typename InIt>
bool ComparePartialString(InIt begin, InIt end, const char* other) {
    for (; begin != end && *other != '\0'; ++begin, ++other) {
        if (*begin != *other) {
            return false;
        }
    }
    // Only return true if both strings finished at the same point
    return (begin == end) == (*other == '\0');
}

/**
 * Creates a std::string from a fixed-size NUL-terminated char buffer. If the buffer isn't
 * NUL-terminated then the string ends at max_len characters.
 */
std::string StringFromFixedZeroTerminatedBuffer(const char* buffer, size_t max_len);
}
