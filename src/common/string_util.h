// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <cstddef>
#include <string>
#include <vector>
#include "common/common_types.h"
#include "common/swap.h"

namespace Common {

/// Make a string lowercase
std::string ToLower(std::string str);

/// Make a string uppercase
std::string ToUpper(std::string str);

std::string StripSpaces(const std::string& s);
std::string StripQuotes(const std::string& s);

std::string StringFromBool(bool value);

std::string TabsToSpaces(int tab_size, std::string in);

void SplitString(const std::string& str, char delim, std::vector<std::string>& output);

// "C:/Windows/winhelp.exe" to "C:/Windows/", "winhelp", ".exe"
bool SplitPath(const std::string& full_path, std::string* _pPath, std::string* _pFilename,
               std::string* _pExtension);

void BuildCompleteFilename(std::string& _CompleteFilename, const std::string& _Path,
                           const std::string& _Filename);
std::string ReplaceAll(std::string result, const std::string& src, const std::string& dest);

std::string UTF16ToUTF8(const std::u16string& input);
std::u16string UTF8ToUTF16(const std::string& input);

#ifdef _WIN32
std::string UTF16ToUTF8(const std::wstring& input);
std::wstring UTF8ToUTF16W(const std::string& str);

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
 * Converts a UTF-16 text in a container to a UTF-8 std::string.
 */
template <typename T>
std::string UTF16BufferToUTF8(const T& text) {
    const auto text_end = std::find(text.begin(), text.end(), u'\0');
    const std::size_t text_size = std::distance(text.begin(), text_end);
    std::u16string buffer(text_size, 0);
    std::transform(text.begin(), text_end, buffer.begin(), [](u16_le character) {
        return static_cast<char16_t>(static_cast<u16>(character));
    });
    return UTF16ToUTF8(buffer);
}

/**
 * Creates a std::string from a fixed-size NUL-terminated char buffer. If the buffer isn't
 * NUL-terminated then the string ends at max_len characters.
 */
std::string StringFromFixedZeroTerminatedBuffer(const char* buffer, std::size_t max_len);

} // namespace Common
