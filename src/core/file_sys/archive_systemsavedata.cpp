// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/disk_archive.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

Archive_SystemSaveData::Archive_SystemSaveData(const std::string& mount_point, u64 save_id)
        : DiskArchive(Common::StringFromFormat("%s%08X/%08X/", mount_point.c_str(),
        static_cast<u32>(save_id & 0xFFFFFFFF), static_cast<u32>((save_id >> 32) & 0xFFFFFFFF))) {
    LOG_INFO(Service_FS, "Directory %s set as SystemSaveData.", this->mount_point.c_str());
}

bool Archive_SystemSaveData::Initialize() {
    if (!FileUtil::CreateFullPath(mount_point)) {
        LOG_ERROR(Service_FS, "Unable to create SystemSaveData path.");
        return false;
    }

    return true;
}

} // namespace FileSys
