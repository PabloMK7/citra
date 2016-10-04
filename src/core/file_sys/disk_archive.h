// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>
#include "common/common_types.h"
#include "common/file_util.h"
#include "core/file_sys/archive_backend.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/file_backend.h"
#include "core/hle/result.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/**
 * Helper which implements a backend accessing the host machine's filesystem.
 * This should be subclassed by concrete archive types, which will provide the
 * base directory on the host filesystem and override any required functionality.
 */
class DiskArchive : public ArchiveBackend {
public:
    DiskArchive(const std::string& mount_point_) : mount_point(mount_point_) {}

    virtual std::string GetName() const override {
        return "DiskArchive: " + mount_point;
    }

    ResultVal<std::unique_ptr<FileBackend>> OpenFile(const Path& path,
                                                     const Mode mode) const override;
    ResultCode DeleteFile(const Path& path) const override;
    bool RenameFile(const Path& src_path, const Path& dest_path) const override;
    bool DeleteDirectory(const Path& path) const override;
    bool DeleteDirectoryRecursively(const Path& path) const override;
    ResultCode CreateFile(const Path& path, u64 size) const override;
    bool CreateDirectory(const Path& path) const override;
    bool RenameDirectory(const Path& src_path, const Path& dest_path) const override;
    std::unique_ptr<DirectoryBackend> OpenDirectory(const Path& path) const override;
    u64 GetFreeBytes() const override;

protected:
    friend class DiskFile;
    friend class DiskDirectory;

    std::string mount_point;
};

class DiskFile : public FileBackend {
public:
    DiskFile(const DiskArchive& archive, const Path& path, const Mode mode);

    ResultCode Open() override;
    ResultVal<size_t> Read(u64 offset, size_t length, u8* buffer) const override;
    ResultVal<size_t> Write(u64 offset, size_t length, bool flush, const u8* buffer) const override;
    u64 GetSize() const override;
    bool SetSize(u64 size) const override;
    bool Close() const override;

    void Flush() const override {
        file->Flush();
    }

protected:
    std::string path;
    Mode mode;
    std::unique_ptr<FileUtil::IOFile> file;
};

class DiskDirectory : public DirectoryBackend {
public:
    DiskDirectory(const DiskArchive& archive, const Path& path);

    ~DiskDirectory() override {
        Close();
    }

    bool Open() override;
    u32 Read(const u32 count, Entry* entries) override;

    bool Close() const override {
        return true;
    }

protected:
    std::string path;
    u32 total_entries_in_directory;
    FileUtil::FSTEntry directory;

    // We need to remember the last entry we returned, so a subsequent call to Read will continue
    // from the next one.  This iterator will always point to the next unread entry.
    std::vector<FileUtil::FSTEntry>::iterator children_iterator;
};

} // namespace FileSys
