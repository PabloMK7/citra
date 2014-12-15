// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/file_sys/archive_backend.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/// File system interface to the SDMC archive
class Archive_SDMC final : public ArchiveBackend {
public:
    Archive_SDMC(const std::string& mount_point);
    ~Archive_SDMC() override;

    /**
     * Initialize the archive.
     * @return true if it initialized successfully
     */
    bool Initialize();

    std::string GetName() const override { return "SDMC"; }

    /**
     * Open a file specified by its path, using the specified mode
     * @param path Path relative to the archive
     * @param mode Mode to open the file with
     * @return Opened file, or nullptr
     */
    std::unique_ptr<FileBackend> OpenFile(const Path& path, const Mode mode) const override;

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
    std::unique_ptr<DirectoryBackend> OpenDirectory(const Path& path) const override;

    /**
     * Getter for the path used for this Archive
     * @return Mount point of that passthrough archive
     */
    std::string GetMountPoint() const;

private:
    std::string mount_point;
};

} // namespace FileSys
