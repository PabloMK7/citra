// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/disk_archive.h"
#include "core/hle/service/fs/archive.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

static std::string GetSystemSaveDataPath(const std::string& mount_point, u64 save_id) {
    u32 save_high = static_cast<u32>((save_id >> 32) & 0xFFFFFFFF);
    u32 save_low = static_cast<u32>(save_id & 0xFFFFFFFF);
    return Common::StringFromFormat("%s%08X/%08X/", mount_point.c_str(), save_low, save_high);
}

static std::string GetSystemSaveDataContainerPath(const std::string& mount_point) {
    return Common::StringFromFormat("%sdata/%32x/sysdata/", mount_point.c_str(), ID0);
}

Archive_SystemSaveData::Archive_SystemSaveData(const std::string& mount_point, u64 save_id)
        : DiskArchive(GetSystemSaveDataPath(GetSystemSaveDataContainerPath(mount_point), save_id)) {
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
