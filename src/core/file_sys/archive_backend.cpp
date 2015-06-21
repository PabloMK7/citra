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
    case Binary:
    {
        u8* data = Memory::GetPointer(pointer);
        binary = std::vector<u8>(data, data + size);
        break;
    }

    case Char:
    {
        const char* data = reinterpret_cast<const char*>(Memory::GetPointer(pointer));
        string = std::string(data, size - 1); // Data is always null-terminated.
        break;
    }

    case Wchar:
    {
        const char16_t* data = reinterpret_cast<const char16_t*>(Memory::GetPointer(pointer));
        u16str = std::u16string(data, size/2 - 1); // Data is always null-terminated.
        break;
    }

    default:
        break;
    }
}

const std::string Path::DebugStr() const {
    switch (GetType()) {
    case Invalid:
    default:
        return "[Invalid]";
    case Empty:
        return "[Empty]";
    case Binary:
    {
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

const std::string Path::AsString() const {
    switch (GetType()) {
    case Char:
        return string;
    case Wchar:
        return Common::UTF16ToUTF8(u16str);
    case Empty:
        return{};
    case Invalid:
    case Binary:
    default:
        // TODO(yuriks): Add assert
        LOG_ERROR(Service_FS, "LowPathType cannot be converted to string!");
        return{};
    }
}

const std::u16string Path::AsU16Str() const {
    switch (GetType()) {
    case Char:
        return Common::UTF8ToUTF16(string);
    case Wchar:
        return u16str;
    case Empty:
        return{};
    case Invalid:
    case Binary:
        // TODO(yuriks): Add assert
        LOG_ERROR(Service_FS, "LowPathType cannot be converted to u16string!");
        return{};
    }
}

const std::vector<u8> Path::AsBinary() const {
    switch (GetType()) {
    case Binary:
        return binary;
    case Char:
        return std::vector<u8>(string.begin(), string.end());
    case Wchar:
    {
        // use two u8 for each character of u16str
        std::vector<u8> to_return(u16str.size() * 2);
        for (size_t i = 0; i < u16str.size(); ++i) {
            u16 tmp_char = u16str.at(i);
            to_return[i*2] = (tmp_char & 0xFF00) >> 8;
            to_return[i*2 + 1] = (tmp_char & 0x00FF);
        }
        return to_return;
    }
    case Empty:
        return{};
    case Invalid:
    default:
        // TODO(yuriks): Add assert
        LOG_ERROR(Service_FS, "LowPathType cannot be converted to binary!");
        return{};
    }
}

}
