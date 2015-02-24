// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/file_sys/disk_archive.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/// File system interface to the SystemSaveData archive
class ArchiveFactory_SystemSaveData final : public ArchiveFactory {
public:
    ArchiveFactory_SystemSaveData(const std::string& mount_point);

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path) override;
    ResultCode Format(const Path& path) override;

    std::string GetName() const override { return "SystemSaveData"; }

private:
    std::string base_path;
};

} // namespace FileSys
