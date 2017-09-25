// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"
#include "core/file_sys/archive_backend.h"
#include "core/hle/result.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

struct NCCHData {
    std::shared_ptr<std::vector<u8>> icon;
    std::shared_ptr<std::vector<u8>> logo;
    std::shared_ptr<std::vector<u8>> banner;
    std::shared_ptr<FileUtil::IOFile> romfs_file;
    u64 romfs_offset = 0;
    u64 romfs_size = 0;

    std::shared_ptr<FileUtil::IOFile> update_romfs_file;
    u64 update_romfs_offset = 0;
    u64 update_romfs_size = 0;
};

/// File system interface to the SelfNCCH archive
class ArchiveFactory_SelfNCCH final : public ArchiveFactory {
public:
    ArchiveFactory_SelfNCCH() = default;

    /// Registers a loaded application so that we can open its SelfNCCH archive when requested.
    void Register(Loader::AppLoader& app_loader);

    std::string GetName() const override {
        return "SelfNCCH";
    }
    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path) override;
    ResultCode Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info) override;
    ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path) const override;

private:
    /// Mapping of ProgramId -> NCCHData
    std::unordered_map<u64, NCCHData> ncch_data;
};

} // namespace FileSys
