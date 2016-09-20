// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <vector>
#include "common/common_types.h"
#include "core/file_sys/archive_backend.h"
#include "core/hle/result.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/// File system interface to the RomFS archive
class ArchiveFactory_RomFS final : public ArchiveFactory {
public:
    ArchiveFactory_RomFS(Loader::AppLoader& app_loader);

    std::string GetName() const override {
        return "RomFS";
    }
    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path) override;
    ResultCode Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info) override;
    ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path) const override;

private:
    std::shared_ptr<FileUtil::IOFile> romfs_file;
    u64 data_offset;
    u64 data_size;
};

} // namespace FileSys
