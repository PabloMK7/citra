// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include "common/common_types.h"
#include "common/string_util.h"
#include "common/bit_field.h"

#include "core/file_sys/file.h"
#include "core/file_sys/directory.h"

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
                return std::vector<u8>(u16str.begin(), u16str.end());
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

class Archive : NonCopyable {
public:
    /// Supported archive types
    enum class IdCode : u32 {
        RomFS               = 0x00000003,
        SaveData            = 0x00000004,
        ExtSaveData         = 0x00000006,
        SharedExtSaveData   = 0x00000007,
        SystemSaveData      = 0x00000008,
        SDMC                = 0x00000009,
        SDMCWriteOnly       = 0x0000000A,
    };

    Archive() { }
    virtual ~Archive() { }

    /**
     * Get the IdCode of the archive (e.g. RomFS, SaveData, etc.)
     * @return IdCode of the archive
     */
    virtual IdCode GetIdCode() const = 0;

    /**
     * Open a file specified by its path, using the specified mode
     * @param path Path relative to the archive
     * @param mode Mode to open the file with
     * @return Opened file, or nullptr
     */
    virtual std::unique_ptr<File> OpenFile(const Path& path, const Mode mode) const = 0;

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
    virtual std::unique_ptr<Directory> OpenDirectory(const Path& path) const = 0;

    /**
     * Read data from the archive
     * @param offset Offset in bytes to start reading data from
     * @param length Length in bytes of data to read from archive
     * @param buffer Buffer to read data into
     * @return Number of bytes read
     */
    virtual size_t Read(const u64 offset, const u32 length, u8* buffer) const = 0;

    /**
     * Write data to the archive
     * @param offset Offset in bytes to start writing data to
     * @param length Length in bytes of data to write to archive
     * @param buffer Buffer to write data from
     * @param flush  The flush parameters (0 == do not flush)
     * @return Number of bytes written
     */
    virtual size_t Write(const u64 offset, const u32 length, const u32 flush, u8* buffer) = 0;

    /**
     * Get the size of the archive in bytes
     * @return Size of the archive in bytes
     */
    virtual size_t GetSize() const = 0;

    /**
     * Set the size of the archive in bytes
     */
    virtual void SetSize(const u64 size) = 0;
};

} // namespace FileSys
