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

namespace FileSys {

class DiskFile : public FileBackend {
public:
    DiskFile(FileUtil::IOFile&& file_, const Mode& mode_,
             std::unique_ptr<DelayGenerator> delay_generator_)
        : file(new FileUtil::IOFile(std::move(file_))) {
        delay_generator = std::move(delay_generator_);
        mode.hex = mode_.hex;
    }

    ResultVal<std::size_t> Read(u64 offset, std::size_t length, u8* buffer) const override;
    ResultVal<std::size_t> Write(u64 offset, std::size_t length, bool flush,
                                 const u8* buffer) override;
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
    explicit DiskDirectory(const std::string& path);

    ~DiskDirectory() override {
        Close();
    }

    u32 Read(u32 count, Entry* entries) override;

    bool Close() const override {
        return true;
    }

protected:
    FileUtil::FSTEntry directory{};

    // We need to remember the last entry we returned, so a subsequent call to Read will continue
    // from the next one.  This iterator will always point to the next unread entry.
    std::vector<FileUtil::FSTEntry>::iterator children_iterator;
};

} // namespace FileSys
