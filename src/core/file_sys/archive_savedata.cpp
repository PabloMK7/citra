// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/archive_savedata.h"
#include "core/file_sys/disk_archive.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

Archive_SaveData::Archive_SaveData(const std::string& mount_point)
        : DiskArchive(mount_point) {
    LOG_INFO(Service_FS, "Directory %s set as SaveData.", this->mount_point.c_str());
}

ResultCode Archive_SaveData::Open(const Path& path) {
    if (concrete_mount_point.empty())
        concrete_mount_point = Common::StringFromFormat("%s%016X", mount_point.c_str(), Kernel::g_program_id) + DIR_SEP;
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
