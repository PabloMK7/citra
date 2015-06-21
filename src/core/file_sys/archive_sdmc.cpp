// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/make_unique.h"

#include "core/file_sys/archive_sdmc.h"
#include "core/file_sys/disk_archive.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

ArchiveFactory_SDMC::ArchiveFactory_SDMC(const std::string& sdmc_directory) : sdmc_directory(sdmc_directory) {
    LOG_INFO(Service_FS, "Directory %s set as SDMC.", sdmc_directory.c_str());
}

bool ArchiveFactory_SDMC::Initialize() {
    if (!Settings::values.use_virtual_sd) {
        LOG_WARNING(Service_FS, "SDMC disabled by config.");
        return false;
    }

    if (!FileUtil::CreateFullPath(sdmc_directory)) {
        LOG_ERROR(Service_FS, "Unable to create SDMC path.");
        return false;
    }

    return true;
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SDMC::Open(const Path& path) {
    auto archive = Common::make_unique<DiskArchive>(sdmc_directory);
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_SDMC::Format(const Path& path) {
    // This is kind of an undesirable operation, so let's just ignore it. :)
    return RESULT_SUCCESS;
}

} // namespace FileSys
