// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "common/common_types.h"
#include "common/string_util.h"
#include "common/bit_field.h"

#include "core/file_sys/file_backend.h"
#include "core/file_sys/directory_backend.h"

#include "core/mem_map.h"
#include "core/hle/kernel/kernel.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

// Path string type
enum LowPathType : u32 {
    Invalid = 0,
    Empty   = 1,
    Binary  = 2,
    Char    = 3,
    Wchar   = 4
};

union Mode {
    u32 hex;
    BitField<0, 1, u32> read_flag;
    BitField<1, 1, u32> write_flag;
    BitField<2, 1, u32> create_flag;
};

class Path {
public:

    Path():
        type(Invalid)
    {
    }

    Path(LowPathType type, u32 size, u32 pointer):
        type(type)
    {
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

    LowPathType GetType() const {
        return type;
    }

    /**
     * Gets the string representation of the path for debugging
     * @return String representation of the path for debugging
     */
    const std::string DebugStr() const {
        switch (GetType()) {
        case Invalid:
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
        default:
            // TODO(yuriks): Add assert
            LOG_ERROR(Service_FS, "LowPathType cannot be converted to string!");
            return {};
        }
    }

    const std::string AsString() const {
        switch (GetType()) {
            case Char:
                return string;
            case Wchar:
                return Common::UTF16ToUTF8(u16str);
            case Empty:
                return {};
            default:
                // TODO(yuriks): Add assert
                LOG_ERROR(Service_FS, "LowPathType cannot be converted to string!");
                return {};
        }
    }

    const std::u16string AsU16Str() const {
        switch (GetType()) {
            case Char:
                return Common::UTF8ToUTF16(string);
            case Wchar:
                return u16str;
            case Empty:
                return {};
            default:
                // TODO(yuriks): Add assert
                LOG_ERROR(Service_FS, "LowPathType cannot be converted to u16string!");
                return {};
        }
    }

    const std::vector<u8> AsBinary() const {
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
                return {};
            default:
                // TODO(yuriks): Add assert
                LOG_ERROR(Service_FS, "LowPathType cannot be converted to binary!");
                return {};
        }
    }

private:
    LowPathType type;
    std::vector<u8> binary;
    std::string string;
    std::u16string u16str;
};

class ArchiveBackend : NonCopyable {
public:
    virtual ~ArchiveBackend() { }

    /**
     * Get a descriptive name for the archive (e.g. "RomFS", "SaveData", etc.)
     */
    virtual std::string GetName() const = 0;

    /**
     * Open a file specified by its path, using the specified mode
     * @param path Path relative to the archive
     * @param mode Mode to open the file with
     * @return Opened file, or nullptr
     */
    virtual std::unique_ptr<FileBackend> OpenFile(const Path& path, const Mode mode) const = 0;

    /**
     * Delete a file specified by its path
     * @param path Path relative to the archive
     * @return Whether the file could be deleted
     */
    virtual bool DeleteFile(const FileSys::Path& path) const = 0;

    /**
     * Rename a File specified by its path
     * @param src_path Source path relative to the archive
     * @param dest_path Destination path relative to the archive
     * @return Whether rename succeeded
     */
    virtual bool RenameFile(const FileSys::Path& src_path, const FileSys::Path& dest_path) const = 0;

    /**
     * Delete a directory specified by its path
     * @param path Path relative to the archive
     * @return Whether the directory could be deleted
     */
    virtual bool DeleteDirectory(const FileSys::Path& path) const = 0;

    /**
     * Create a directory specified by its path
     * @param path Path relative to the archive
     * @return Whether the directory could be created
     */
    virtual bool CreateDirectory(const Path& path) const = 0;

    /**
     * Rename a Directory specified by its path
     * @param src_path Source path relative to the archive
     * @param dest_path Destination path relative to the archive
     * @return Whether rename succeeded
     */
    virtual bool RenameDirectory(const FileSys::Path& src_path, const FileSys::Path& dest_path) const = 0;

    /**
     * Open a directory specified by its path
     * @param path Path relative to the archive
     * @return Opened directory, or nullptr
     */
    virtual std::unique_ptr<DirectoryBackend> OpenDirectory(const Path& path) const = 0;
};

} // namespace FileSys
