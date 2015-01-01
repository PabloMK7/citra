// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"

#include "core/file_sys/archive_backend.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

class IVFCArchive : public ArchiveBackend {
public:
    IVFCArchive();

    std::unique_ptr<FileBackend> OpenFile(const Path& path, const Mode mode) const override;
    bool DeleteFile(const Path& path) const override;
    bool RenameFile(const Path& src_path, const Path& dest_path) const override;
    bool DeleteDirectory(const Path& path) const override;
    ResultCode CreateFile(const Path& path, u32 size) const override;
    bool CreateDirectory(const Path& path) const override;
    bool RenameDirectory(const Path& src_path, const Path& dest_path) const override;
    std::unique_ptr<DirectoryBackend> OpenDirectory(const Path& path) const override;
    ResultCode Format(const Path& path) const override;

protected:
    friend class IVFCFile;
    std::vector<u8> raw_data;
};

class IVFCFile : public FileBackend {
public:
    IVFCFile(const IVFCArchive* archive) : archive(archive) {}

    bool Open() override { return true; }
    size_t Read(const u64 offset, const u32 length, u8* buffer) const override;
    size_t Write(const u64 offset, const u32 length, const u32 flush, const u8* buffer) const override;
    size_t GetSize() const override;
    bool SetSize(const u64 size) const override;
    bool Close() const override { return false; }
    void Flush() const override { }

private:
    const IVFCArchive* archive;
};

class IVFCDirectory : public DirectoryBackend {
public:
    IVFCDirectory() { }

    bool Open() override { return false; }
    u32 Read(const u32 count, Entry* entries) override { return 0; }
    bool Close() const override { return false; }
};

} // namespace FileSys
