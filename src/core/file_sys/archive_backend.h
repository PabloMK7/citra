// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/result.h"

namespace FileSys {

class FileBackend;
class DirectoryBackend;

// Path string type
enum LowPathType : u32 {
    Invalid = 0,
    Empty = 1,
    Binary = 2,
    Char = 3,
    Wchar = 4,
};

union Mode {
    u32 hex;
    BitField<0, 1, u32> read_flag;
    BitField<1, 1, u32> write_flag;
    BitField<2, 1, u32> create_flag;
};

class Path {
public:
    Path() : type(Invalid) {}
    Path(const char* path) : type(Char), string(path) {}
    Path(std::vector<u8> binary_data) : type(Binary), binary(std::move(binary_data)) {}
    Path(LowPathType type, u32 size, u32 pointer);

    LowPathType GetType() const {
        return type;
    }

    /**
     * Gets the string representation of the path for debugging
     * @return String representation of the path for debugging
     */
    std::string DebugStr() const;

    std::string AsString() const;
    std::u16string AsU16Str() const;
    std::vector<u8> AsBinary() const;

private:
    LowPathType type;
    std::vector<u8> binary;
    std::string string;
    std::u16string u16str;
};

/// Parameters of the archive, as specified in the Create or Format call.
struct ArchiveFormatInfo {
    u32_le total_size;         ///< The pre-defined size of the archive.
    u32_le number_directories; ///< The pre-defined number of directories in the archive.
    u32_le number_files;       ///< The pre-defined number of files in the archive.
    u8 duplicate_data;         ///< Whether the archive should duplicate the data.
};
static_assert(std::is_pod<ArchiveFormatInfo>::value, "ArchiveFormatInfo is not POD");

class ArchiveBackend : NonCopyable {
public:
    virtual ~ArchiveBackend() {}

    /**
     * Get a descriptive name for the archive (e.g. "RomFS", "SaveData", etc.)
     */
    virtual std::string GetName() const = 0;

    /**
     * Open a file specified by its path, using the specified mode
     * @param path Path relative to the archive
     * @param mode Mode to open the file with
     * @return Opened file, or error code
     */
    virtual ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path,
                                                             const Mode mode) const = 0;

    /**
     * Delete a file specified by its path
     * @param path Path relative to the archive
     * @return Result of the operation
     */
    virtual ResultCode DeleteFile(const Path& path) const = 0;

    /**
     * Rename a File specified by its path
     * @param src_path Source path relative to the archive
     * @param dest_path Destination path relative to the archive
     * @return Whether rename succeeded
     */
    virtual bool RenameFile(const Path& src_path, const Path& dest_path) const = 0;

    /**
     * Delete a directory specified by its path
     * @param path Path relative to the archive
     * @return Whether the directory could be deleted
     */
    virtual bool DeleteDirectory(const Path& path) const = 0;

    /**
     * Delete a directory specified by its path and anything under it
     * @param path Path relative to the archive
     * @return Whether the directory could be deleted
     */
    virtual bool DeleteDirectoryRecursively(const Path& path) const = 0;

    /**
     * Create a file specified by its path
     * @param path Path relative to the Archive
     * @param size The size of the new file, filled with zeroes
     * @return File creation result code
     */
    virtual ResultCode CreateFile(const Path& path, u64 size) const = 0;

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
    virtual bool RenameDirectory(const Path& src_path, const Path& dest_path) const = 0;

    /**
     * Open a directory specified by its path
     * @param path Path relative to the archive
     * @return Opened directory, or nullptr
     */
    virtual std::unique_ptr<DirectoryBackend> OpenDirectory(const Path& path) const = 0;

    /**
     * Get the free space
     * @return The number of free bytes in the archive
     */
    virtual u64 GetFreeBytes() const = 0;
};

class ArchiveFactory : NonCopyable {
public:
    virtual ~ArchiveFactory() {}

    /**
     * Get a descriptive name for the archive (e.g. "RomFS", "SaveData", etc.)
     */
    virtual std::string GetName() const = 0;

    /**
     * Tries to open the archive of this type with the specified path
     * @param path Path to the archive
     * @return An ArchiveBackend corresponding operating specified archive path.
     */
    virtual ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path) = 0;

    /**
     * Deletes the archive contents and then re-creates the base folder
     * @param path Path to the archive
     * @param format_info Format information for the new archive
     * @return ResultCode of the operation, 0 on success
     */
    virtual ResultCode Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info) = 0;

    /**
     * Retrieves the format info about the archive with the specified path
     * @param path Path to the archive
     * @return Format information about the archive or error code
     */
    virtual ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path) const = 0;
};

} // namespace FileSys
