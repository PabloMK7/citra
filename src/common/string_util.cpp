// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cctype>
#include <cerrno>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <boost/range/algorithm/transform.hpp>
#include "common/common_paths.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#ifdef _MSC_VER
#include <codecvt>
#include <Windows.h>
#include "common/common_funcs.h"
#else
#include <iconv.h>
#endif

namespace Common {

/// Make a string lowercase
std::string ToLower(std::string str) {
    boost::transform(str, str.begin(), ::tolower);
    return str;
}

/// Make a string uppercase
std::string ToUpper(std::string str) {
    boost::transform(str, str.begin(), ::toupper);
    return str;
}

// faster than sscanf
bool AsciiToHex(const char* _szValue, u32& result) {
    char* endptr = nullptr;
    const u32 value = strtoul(_szValue, &endptr, 16);

    if (!endptr || *endptr)
        return false;

    result = value;
    return true;
}

bool CharArrayFromFormatV(char* out, int outsize, const char* format, va_list args) {
    int writtenCount;

#ifdef _MSC_VER
    // You would think *printf are simple, right? Iterate on each character,
    // if it's a format specifier handle it properly, etc.
    //
    // Nooooo. Not according to the C standard.
    //
    // According to the C99 standard (7.19.6.1 "The fprintf function")
    //     The format shall be a multibyte character sequence
    //
    // Because some character encodings might have '%' signs in the middle of
    // a multibyte sequence (SJIS for example only specifies that the first
    // byte of a 2 byte sequence is "high", the second byte can be anything),
    // printf functions have to decode the multibyte sequences and try their
    // best to not screw up.
    //
    // Unfortunately, on Windows, the locale for most languages is not UTF-8
    // as we would need. Notably, for zh_TW, Windows chooses EUC-CN as the
    // locale, and completely fails when trying to decode UTF-8 as EUC-CN.
    //
    // On the other hand, the fix is simple: because we use UTF-8, no such
    // multibyte handling is required as we can simply assume that no '%' char
    // will be present in the middle of a multibyte sequence.
    //
    // This is why we lookup an ANSI (cp1252) locale here and use _vsnprintf_l.
    static locale_t c_locale = nullptr;
    if (!c_locale)
        c_locale = _create_locale(LC_ALL, ".1252");
    writtenCount = _vsnprintf_l(out, outsize, format, c_locale, args);
#else
    writtenCount = vsnprintf(out, outsize, format, args);
#endif

    if (writtenCount > 0 && writtenCount < outsize) {
        out[writtenCount] = '\0';
        return true;
    } else {
        out[outsize - 1] = '\0';
        return false;
    }
}

std::string StringFromFormat(const char* format, ...) {
    va_list args;
    char* buf = nullptr;
#ifdef _WIN32
    int required = 0;

    va_start(args, format);
    required = _vscprintf(format, args);
    buf = new char[required + 1];
    CharArrayFromFormatV(buf, required + 1, format, args);
    va_end(args);

    std::string temp = buf;
    delete[] buf;
#else
    va_start(args, format);
    if (vasprintf(&buf, format, args) < 0)
        LOG_ERROR(Common, "Unable to allocate memory for string");
    va_end(args);

    std::string temp = buf;
    free(buf);
#endif
    return temp;
}

// For Debugging. Read out an u8 array.
std::string ArrayToString(const u8* data, u32 size, int line_len, bool spaces) {
    std::ostringstream oss;
    oss << std::setfill('0') << std::hex;

    for (int line = 0; size; ++data, --size) {
        oss << std::setw(2) << (int)*data;

        if (line_len == ++line) {
            oss << '\n';
            line = 0;
        } else if (spaces)
            oss << ' ';
    }

    return oss.str();
}

// Turns "  hej " into "hej". Also handles tabs.
std::string StripSpaces(const std::string& str) {
    const size_t s = str.find_first_not_of(" \t\r\n");

    if (str.npos != s)
        return str.substr(s, str.find_last_not_of(" \t\r\n") - s + 1);
    else
        return "";
}

// "\"hello\"" is turned to "hello"
// This one assumes that the string has already been space stripped in both
// ends, as done by StripSpaces above, for example.
std::string StripQuotes(const std::string& s) {
    if (s.size() && '\"' == s[0] && '\"' == *s.rbegin())
        return s.substr(1, s.size() - 2);
    else
        return s;
}

bool TryParse(const std::string& str, u32* const output) {
    char* endptr = nullptr;

    // Reset errno to a value other than ERANGE
    errno = 0;

    unsigned long value = strtoul(str.c_str(), &endptr, 0);

    if (!endptr || *endptr)
        return false;

    if (errno == ERANGE)
        return false;

#if ULONG_MAX > UINT_MAX
    if (value >= 0x100000000ull && value <= 0xFFFFFFFF00000000ull)
        return false;
#endif

    *output = static_cast<u32>(value);
    return true;
}

bool TryParse(const std::string& str, bool* const output) {
    if ("1" == str || "true" == ToLower(str))
        *output = true;
    else if ("0" == str || "false" == ToLower(str))
        *output = false;
    else
        return false;

    return true;
}

std::string StringFromBool(bool value) {
    return value ? "True" : "False";
}

bool SplitPath(const std::string& full_path, std::string* _pPath, std::string* _pFilename,
               std::string* _pExtension) {
    if (full_path.empty())
        return false;

    size_t dir_end = full_path.find_last_of("/"
// windows needs the : included for something like just "C:" to be considered a directory
#ifdef _WIN32
                                            ":"
#endif
                                            );
    if (std::string::npos == dir_end)
        dir_end = 0;
    else
        dir_end += 1;

    size_t fname_end = full_path.rfind('.');
    if (fname_end < dir_end || std::string::npos == fname_end)
        fname_end = full_path.size();

    if (_pPath)
        *_pPath = full_path.substr(0, dir_end);

    if (_pFilename)
        *_pFilename = full_path.substr(dir_end, fname_end - dir_end);

    if (_pExtension)
        *_pExtension = full_path.substr(fname_end);

    return true;
}

void BuildCompleteFilename(std::string& _CompleteFilename, const std::string& _Path,
                           const std::string& _Filename) {
    _CompleteFilename = _Path;

    // check for seperator
    if (DIR_SEP_CHR != *_CompleteFilename.rbegin())
        _CompleteFilename += DIR_SEP_CHR;

    // add the filename
    _CompleteFilename += _Filename;
}

void SplitString(const std::string& str, const char delim, std::vector<std::string>& output) {
    std::istringstream iss(str);
    output.resize(1);

    while (std::getline(iss, *output.rbegin(), delim))
        output.push_back("");

    output.pop_back();
}

std::string TabsToSpaces(int tab_size, const std::string& in) {
    const std::string spaces(tab_size, ' ');
    std::string out(in);

    size_t i = 0;
    while (out.npos != (i = out.find('\t')))
        out.replace(i, 1, spaces);

    return out;
}

std::string ReplaceAll(std::string result, const std::string& src, const std::string& dest) {
    size_t pos = 0;

    if (src == dest)
        return result;

    while ((pos = result.find(src, pos)) != std::string::npos) {
        result.replace(pos, src.size(), dest);
        pos += dest.length();
    }

    return result;
}

#ifdef _MSC_VER

std::string UTF16ToUTF8(const std::u16string& input) {
#if _MSC_VER >= 1900
    // Workaround for missing char16_t/char32_t instantiations in MSVC2015
    std::wstring_convert<std::codecvt_utf8_utf16<__int16>, __int16> convert;
    std::basic_string<__int16> tmp_buffer(input.cbegin(), input.cend());
    return convert.to_bytes(tmp_buffer);
#else
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.to_bytes(input);
#endif
}

std::u16string UTF8ToUTF16(const std::string& input) {
#if _MSC_VER >= 1900
    // Workaround for missing char16_t/char32_t instantiations in MSVC2015
    std::wstring_convert<std::codecvt_utf8_utf16<__int16>, __int16> convert;
    auto tmp_buffer = convert.from_bytes(input);
    return std::u16string(tmp_buffer.cbegin(), tmp_buffer.cend());
#else
    std::wstring_convert<std::codecvt_utf8_utf16<char16_t>, char16_t> convert;
    return convert.from_bytes(input);
#endif
}

static std::wstring CPToUTF16(u32 code_page, const std::string& input) {
    auto const size =
        MultiByteToWideChar(code_page, 0, input.data(), static_cast<int>(input.size()), nullptr, 0);

    std::wstring output;
    output.resize(size);

    if (size == 0 ||
        size != MultiByteToWideChar(code_page, 0, input.data(), static_cast<int>(input.size()),
                                    &output[0], static_cast<int>(output.size())))
        output.clear();

    return output;
}

std::string UTF16ToUTF8(const std::wstring& input) {
    auto const size = WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                                          nullptr, 0, nullptr, nullptr);

    std::string output;
    output.resize(size);

    if (size == 0 ||
        size != WideCharToMultiByte(CP_UTF8, 0, input.data(), static_cast<int>(input.size()),
                                    &output[0], static_cast<int>(output.size()), nullptr, nullptr))
        output.clear();

    return output;
}

std::wstring UTF8ToUTF16W(const std::string& input) {
    return CPToUTF16(CP_UTF8, input);
}

std::string SHIFTJISToUTF8(const std::string& input) {
    return UTF16ToUTF8(CPToUTF16(932, input));
}

std::string CP1252ToUTF8(const std::string& input) {
    return UTF16ToUTF8(CPToUTF16(1252, input));
}

#else

template <typename T>
static std::string CodeToUTF8(const char* fromcode, const std::basic_string<T>& input) {
    std::string result;

    iconv_t const conv_desc = iconv_open("UTF-8", fromcode);
    if ((iconv_t)(-1) == conv_desc) {
        LOG_ERROR(Common, "Iconv initialization failure [%s]: %s", fromcode, strerror(errno));
        iconv_close(conv_desc);
        return {};
    }

    const size_t in_bytes = sizeof(T) * input.size();
    // Multiply by 4, which is the max number of bytes to encode a codepoint
    const size_t out_buffer_size = 4 * in_bytes;

    std::string out_buffer;
    out_buffer.resize(out_buffer_size);

    auto src_buffer = &input[0];
    size_t src_bytes = in_bytes;
    auto dst_buffer = &out_buffer[0];
    size_t dst_bytes = out_buffer.size();

    while (0 != src_bytes) {
        size_t const iconv_result =
            iconv(conv_desc, (char**)(&src_buffer), &src_bytes, &dst_buffer, &dst_bytes);

        if (static_cast<size_t>(-1) == iconv_result) {
            if (EILSEQ == errno || EINVAL == errno) {
                // Try to skip the bad character
                if (0 != src_bytes) {
                    --src_bytes;
                    ++src_buffer;
                }
            } else {
                LOG_ERROR(Common, "iconv failure [%s]: %s", fromcode, strerror(errno));
                break;
            }
        }
    }

    out_buffer.resize(out_buffer_size - dst_bytes);
    out_buffer.swap(result);

    iconv_close(conv_desc);

    return result;
}

std::u16string UTF8ToUTF16(const std::string& input) {
    std::u16string result;

    iconv_t const conv_desc = iconv_open("UTF-16LE", "UTF-8");
    if ((iconv_t)(-1) == conv_desc) {
        LOG_ERROR(Common, "Iconv initialization failure [UTF-8]: %s", strerror(errno));
        iconv_close(conv_desc);
        return {};
    }

    const size_t in_bytes = sizeof(char) * input.size();
    // Multiply by 4, which is the max number of bytes to encode a codepoint
    const size_t out_buffer_size = 4 * sizeof(char16_t) * in_bytes;

    std::u16string out_buffer;
    out_buffer.resize(out_buffer_size);

    char* src_buffer = const_cast<char*>(&input[0]);
    size_t src_bytes = in_bytes;
    char* dst_buffer = (char*)(&out_buffer[0]);
    size_t dst_bytes = out_buffer.size();

    while (0 != src_bytes) {
        size_t const iconv_result =
            iconv(conv_desc, &src_buffer, &src_bytes, &dst_buffer, &dst_bytes);

        if (static_cast<size_t>(-1) == iconv_result) {
            if (EILSEQ == errno || EINVAL == errno) {
                // Try to skip the bad character
                if (0 != src_bytes) {
                    --src_bytes;
                    ++src_buffer;
                }
            } else {
                LOG_ERROR(Common, "iconv failure [UTF-8]: %s", strerror(errno));
                break;
            }
        }
    }

    out_buffer.resize(out_buffer_size - dst_bytes);
    out_buffer.swap(result);

    iconv_close(conv_desc);

    return result;
}

std::string UTF16ToUTF8(const std::u16string& input) {
    return CodeToUTF8("UTF-16LE", input);
}

std::string CP1252ToUTF8(const std::string& input) {
    // return CodeToUTF8("CP1252//TRANSLIT", input);
    // return CodeToUTF8("CP1252//IGNORE", input);
    return CodeToUTF8("CP1252", input);
}

std::string SHIFTJISToUTF8(const std::string& input) {
    // return CodeToUTF8("CP932", input);
    return CodeToUTF8("SJIS", input);
}

#endif

std::string StringFromFixedZeroTerminatedBuffer(const char* buffer, size_t max_len) {
    size_t len = 0;
    while (len < max_len && buffer[len] != '\0')
        ++len;

    return std::string(buffer, len);
}
}
