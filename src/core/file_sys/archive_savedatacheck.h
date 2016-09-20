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

/// File system interface to the SaveDataCheck archive
class ArchiveFactory_SaveDataCheck final : public ArchiveFactory {
public:
    ArchiveFactory_SaveDataCheck(const std::string& mount_point);

    std::string GetName() const override {
        return "SaveDataCheck";
    }

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path) override;
    ResultCode Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info) override;
    ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path) const override;

private:
    std::string mount_point;
};

} // namespace FileSys
