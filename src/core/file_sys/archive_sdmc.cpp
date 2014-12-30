// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/archive_sdmc.h"
#include "core/file_sys/disk_archive.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

Archive_SDMC::Archive_SDMC(const std::string& mount_point) : DiskArchive(mount_point) {
    LOG_INFO(Service_FS, "Directory %s set as SDMC.", mount_point.c_str());
}

bool Archive_SDMC::Initialize() {
    if (!Settings::values.use_virtual_sd) {
        LOG_WARNING(Service_FS, "SDMC disabled by config.");
        return false;
    }

    if (!FileUtil::CreateFullPath(mount_point)) {
        LOG_ERROR(Service_FS, "Unable to create SDMC path.");
        return false;
    }

    return true;
}

} // namespace FileSys
