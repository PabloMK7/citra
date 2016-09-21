// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include <iomanip>
#include <sstream>
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/file_sys/archive_backend.h"
#include "core/memory.h"

namespace FileSys {

Path::Path(LowPathType type, u32 size, u32 pointer) : type(type) {
    switch (type) {
    case Binary: {
        binary.resize(size);
        Memory::ReadBlock(pointer, binary.data(), binary.size());
        break;
    }

    case Char: {
        string.resize(size - 1); // Data is always null-terminated.
        Memory::ReadBlock(pointer, &string[0], string.size());
        break;
    }

    case Wchar: {
        u16str.resize(size / 2 - 1); // Data is always null-terminated.
        Memory::ReadBlock(pointer, &u16str[0], u16str.size() * sizeof(char16_t));
        break;
    }

    default:
        break;
    }
}

std::string Path::DebugStr() const {
    switch (GetType()) {
    case Invalid:
    default:
        return "[Invalid]";
    case Empty:
        return "[Empty]";
    case Binary: {
        std::stringstream res;
        res << "[Binary: ";
        for (unsigned byte : binary)
            res << std::hex << std::setw(2) << std::setfill('0') << byte;
        res << ']';
        return res.str();
    }
    case Char:
        return "[Char: " + AsString() + ']';
    case Wchar:
        return "[Wchar: " + AsString() + ']';
    }
}

std::string Path::AsString() const {
    switch (GetType()) {
    case Char:
        return string;
    case Wchar:
        return Common::UTF16ToUTF8(u16str);
    case Empty:
        return {};
    case Invalid:
    case Binary:
    default:
        // TODO(yuriks): Add assert
        LOG_ERROR(Service_FS, "LowPathType cannot be converted to string!");
        return {};
    }
}

std::u16string Path::AsU16Str() const {
    switch (GetType()) {
    case Char:
        return Common::UTF8ToUTF16(string);
    case Wchar:
        return u16str;
    case Empty:
        return {};
    case Invalid:
    case Binary:
        // TODO(yuriks): Add assert
        LOG_ERROR(Service_FS, "LowPathType cannot be converted to u16string!");
        return {};
    }
}

std::vector<u8> Path::AsBinary() const {
    switch (GetType()) {
    case Binary:
        return binary;
    case Char:
        return std::vector<u8>(string.begin(), string.end());
    case Wchar: {
        // use two u8 for each character of u16str
        std::vector<u8> to_return(u16str.size() * 2);
        for (size_t i = 0; i < u16str.size(); ++i) {
            u16 tmp_char = u16str.at(i);
            to_return[i * 2] = (tmp_char & 0xFF00) >> 8;
            to_return[i * 2 + 1] = (tmp_char & 0x00FF);
        }
        return to_return;
    }
    case Empty:
        return {};
    case Invalid:
    default:
        // TODO(yuriks): Add assert
        LOG_ERROR(Service_FS, "LowPathType cannot be converted to binary!");
        return {};
    }
}
}
