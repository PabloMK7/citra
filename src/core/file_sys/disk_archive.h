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

class DiskFile : public FileBackend {
public:
    DiskFile(FileUtil::IOFile&& file_, const Mode& mode_)
        : file(new FileUtil::IOFile(std::move(file_))) {
        mode.hex = mode_.hex;
    }

    ResultVal<size_t> Read(u64 offset, size_t length, u8* buffer) const override;
    ResultVal<size_t> Write(u64 offset, size_t length, bool flush, const u8* buffer) const override;
    u64 GetSize() const override;
    bool SetSize(u64 size) const override;
    bool Close() const override;

    void Flush() const override {
        file->Flush();
    }

protected:
    Mode mode;
    std::unique_ptr<FileUtil::IOFile> file;
};

class DiskDirectory : public DirectoryBackend {
public:
    DiskDirectory(const std::string& path);

    ~DiskDirectory() override {
        Close();
    }

    u32 Read(const u32 count, Entry* entries) override;

    bool Close() const override {
        return true;
    }

protected:
    u32 total_entries_in_directory;
    FileUtil::FSTEntry directory;

    // We need to remember the last entry we returned, so a subsequent call to Read will continue
    // from the next one.  This iterator will always point to the next unread entry.
    std::vector<FileUtil::FSTEntry>::iterator children_iterator;
};

} // namespace FileSys
