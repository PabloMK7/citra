// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/archive_savedata.h"
#include "core/file_sys/disk_archive.h"
#include "core/hle/service/fs/archive.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

static std::string GetSaveDataContainerPath(const std::string& mount_point) {
    return Common::StringFromFormat("%sNintendo 3DS/%32x/%32x/title/", mount_point.c_str(), ID0, ID1);
}

static std::string GetSaveDataPath(const std::string& mount_point, u64 program_id) {
    u32 high = program_id >> 32;
    u32 low = program_id & 0xFFFFFFFF;
    return Common::StringFromFormat("%s%08x/%08x/data/00000001/", mount_point.c_str(), high, low);
}

Archive_SaveData::Archive_SaveData(const std::string& mount_point)
        : DiskArchive(GetSaveDataContainerPath(mount_point)) {
    LOG_INFO(Service_FS, "Directory %s set as SaveData.", this->mount_point.c_str());
}

ResultCode Archive_SaveData::Open(const Path& path) {
    if (concrete_mount_point.empty())
        concrete_mount_point = GetSaveDataPath(mount_point, Kernel::g_program_id);
    if (!FileUtil::Exists(concrete_mount_point)) {
        // When a SaveData archive is created for the first time, it is not yet formatted
        // and the save file/directory structure expected by the game has not yet been initialized. 
        // Returning the NotFormatted error code will signal the game to provision the SaveData archive 
        // with the files and folders that it expects. 
        return ResultCode(ErrorDescription::FS_NotFormatted, ErrorModule::FS,
            ErrorSummary::InvalidState, ErrorLevel::Status);
    }
    return RESULT_SUCCESS;
}

ResultCode Archive_SaveData::Format(const Path& path) const {
    FileUtil::DeleteDirRecursively(concrete_mount_point);
    FileUtil::CreateFullPath(concrete_mount_point);
    return RESULT_SUCCESS;
}

} // namespace FileSys
