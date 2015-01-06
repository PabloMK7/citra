// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/disk_archive.h"
#include "core/hle/service/fs/archive.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

static std::string GetExtSaveDataPath(const std::string& mount_point, const Path& path) {
    std::vector<u8> vec_data = path.AsBinary();
    const u32* data = reinterpret_cast<const u32*>(vec_data.data());
    u32 save_low = data[1];
    u32 save_high = data[2];
    return Common::StringFromFormat("%s%08X/%08X/", mount_point.c_str(), save_high, save_low);
}

static std::string GetExtDataContainerPath(const std::string& mount_point, bool shared) {
    if (shared)
        return Common::StringFromFormat("%sdata/%s/extdata/", mount_point.c_str(), SYSTEM_ID.c_str());
    
    return Common::StringFromFormat("%sNintendo 3DS/%s/%s/extdata/", mount_point.c_str(), 
            SYSTEM_ID.c_str(), SDCARD_ID.c_str());
}

Archive_ExtSaveData::Archive_ExtSaveData(const std::string& mount_location, bool shared)
        : DiskArchive(GetExtDataContainerPath(mount_location, shared)), concrete_mount_point(mount_point) {
    LOG_INFO(Service_FS, "Directory %s set as base for ExtSaveData.", mount_point.c_str());
}

bool Archive_ExtSaveData::Initialize() {
    if (!FileUtil::CreateFullPath(mount_point)) {
        LOG_ERROR(Service_FS, "Unable to create ExtSaveData base path.");
        return false;
    }

    return true;
}

ResultCode Archive_ExtSaveData::Open(const Path& path) {
    std::string fullpath = GetExtSaveDataPath(mount_point, path);
    if (!FileUtil::Exists(fullpath)) {
        // TODO(Subv): Check error code, this one is probably wrong
        return ResultCode(ErrorDescription::FS_NotFormatted, ErrorModule::FS,
            ErrorSummary::InvalidState, ErrorLevel::Status);
    }
    concrete_mount_point = fullpath;
    return RESULT_SUCCESS;
}

ResultCode Archive_ExtSaveData::Format(const Path& path) const {
    std::string fullpath = GetExtSaveDataPath(mount_point, path);
    FileUtil::CreateFullPath(fullpath);
    return RESULT_SUCCESS;
}

} // namespace FileSys
