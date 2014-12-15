// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"

#include "core/file_sys/archive.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/// File system interface to the RomFS archive
class Archive_RomFS final : public Archive {
public:
    Archive_RomFS(const Loader::AppLoader& app_loader);
    ~Archive_RomFS() override;

    std::string GetName() const override { return "RomFS"; }

    /**
     * Open a file specified by its path, using the specified mode
     * @param path Path relative to the archive
     * @param mode Mode to open the file with
     * @return Opened file, or nullptr
     */
    std::unique_ptr<File> OpenFile(const Path& path, const Mode mode) const override;

    /**
     * Delete a file specified by its path
     * @param path Path relative to the archive
     * @return Whether the file could be deleted
     */
    bool DeleteFile(const FileSys::Path& path) const override;

    /**
     * Rename a File specified by its path
     * @param src_path Source path relative to the archive
     * @param dest_path Destination path relative to the archive
     * @return Whether rename succeeded
     */
    bool RenameFile(const FileSys::Path& src_path, const FileSys::Path& dest_path) const override;

    /**
     * Delete a directory specified by its path
     * @param path Path relative to the archive
     * @return Whether the directory could be deleted
     */
    bool DeleteDirectory(const FileSys::Path& path) const override;

    /**
     * Create a directory specified by its path
     * @param path Path relative to the archive
     * @return Whether the directory could be created
     */
    bool CreateDirectory(const Path& path) const override;

    /**
     * Rename a Directory specified by its path
     * @param src_path Source path relative to the archive
     * @param dest_path Destination path relative to the archive
     * @return Whether rename succeeded
     */
    bool RenameDirectory(const FileSys::Path& src_path, const FileSys::Path& dest_path) const override;

    /**
     * Open a directory specified by its path
     * @param path Path relative to the archive
     * @return Opened directory, or nullptr
     */
    std::unique_ptr<Directory> OpenDirectory(const Path& path) const override;

    /**
     * Read data from the archive
     * @param offset Offset in bytes to start reading data from
     * @param length Length in bytes of data to read from archive
     * @param buffer Buffer to read data into
     * @return Number of bytes read
     */
    size_t Read(const u64 offset, const u32 length, u8* buffer) const override;

    /**
     * Write data to the archive
     * @param offset Offset in bytes to start writing data to
     * @param length Length in bytes of data to write to archive
     * @param buffer Buffer to write data from
     * @param flush  The flush parameters (0 == do not flush)
     * @return Number of bytes written
     */
    size_t Write(const u64 offset, const u32 length, const u32 flush, u8* buffer) override;

    /**
     * Get the size of the archive in bytes
     * @return Size of the archive in bytes
     */
    size_t GetSize() const override;

    /**
     * Set the size of the archive in bytes
     */
    void SetSize(const u64 size) override;

private:
    std::vector<u8> raw_data;
};

} // namespace FileSys
