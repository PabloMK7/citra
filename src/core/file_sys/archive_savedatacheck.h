// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>

#include "common/common_types.h"

#include "core/file_sys/ivfc_archive.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/// File system interface to the SaveDataCheck archive
class ArchiveFactory_SaveDataCheck final : public ArchiveFactory {
public:
    ArchiveFactory_SaveDataCheck(const std::string& mount_point);

    std::string GetName() const override { return "SaveDataCheck"; }

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path) override;
    ResultCode Format(const Path& path) override;

private:
    std::string mount_point;
};

} // namespace FileSys
