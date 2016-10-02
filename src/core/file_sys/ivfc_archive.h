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
 * Helper which implements an interface to deal with IVFC images used in some archives
 * This should be subclassed by concrete archive types, which will provide the
 * input data (load the raw IVFC archive) and override any required methods
 */
class IVFCArchive : public ArchiveBackend {
public:
    IVFCArchive(std::shared_ptr<FileUtil::IOFile> file, u64 offset, u64 size)
        : romfs_file(file), data_offset(offset), data_size(size) {}

    std::string GetName() const override;

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
    std::shared_ptr<FileUtil::IOFile> romfs_file;
    u64 data_offset;
    u64 data_size;
};

class IVFCFile : public FileBackend {
public:
    IVFCFile(std::shared_ptr<FileUtil::IOFile> file, u64 offset, u64 size)
        : romfs_file(file), data_offset(offset), data_size(size) {}

    ResultCode Open() override {
        return RESULT_SUCCESS;
    }
    ResultVal<size_t> Read(u64 offset, size_t length, u8* buffer) const override;
    ResultVal<size_t> Write(u64 offset, size_t length, bool flush, const u8* buffer) const override;
    u64 GetSize() const override;
    bool SetSize(u64 size) const override;
    bool Close() const override {
        return false;
    }
    void Flush() const override {}

private:
    std::shared_ptr<FileUtil::IOFile> romfs_file;
    u64 data_offset;
    u64 data_size;
};

class IVFCDirectory : public DirectoryBackend {
public:
    bool Open() override {
        return false;
    }
    u32 Read(const u32 count, Entry* entries) override {
        return 0;
    }
    bool Close() const override {
        return false;
    }
};

} // namespace FileSys
