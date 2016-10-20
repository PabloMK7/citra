// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "core/file_sys/archive_backend.h"
#include "core/hle/result.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/// Archive backend for SDMC archive
class SDMCArchive : public ArchiveBackend {
public:
    SDMCArchive(const std::string& mount_point_) : mount_point(mount_point_) {}

    std::string GetName() const override {
        return "SDMCArchive: " + mount_point;
    }

    ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path,
                                                     const Mode& mode) const override;
    ResultCode DeleteFile(const Path& path) const override;
    ResultCode RenameFile(const Path& src_path, const Path& dest_path) const override;
    ResultCode DeleteDirectory(const Path& path) const override;
    ResultCode DeleteDirectoryRecursively(const Path& path) const override;
    ResultCode CreateFile(const Path& path, u64 size) const override;
    ResultCode CreateDirectory(const Path& path) const override;
    ResultCode RenameDirectory(const Path& src_path, const Path& dest_path) const override;
    ResultVal<std::unique_ptr<DirectoryBackend>> OpenDirectory(const Path& path) const override;
    u64 GetFreeBytes() const override;

protected:
    ResultVal<std::unique_ptr<FileBackend>> OpenFileBase(const Path& path, const Mode& mode) const;
    std::string mount_point;
};

/// File system interface to the SDMC archive
class ArchiveFactory_SDMC final : public ArchiveFactory {
public:
    ArchiveFactory_SDMC(const std::string& mount_point);

    /**
     * Initialize the archive.
     * @return true if it initialized successfully
     */
    bool Initialize();

    std::string GetName() const override {
        return "SDMC";
    }

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path) override;
    ResultCode Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info) override;
    ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path) const override;

private:
    std::string sdmc_directory;
};

} // namespace FileSys
