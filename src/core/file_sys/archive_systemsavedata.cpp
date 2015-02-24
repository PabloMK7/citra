// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"
#include "common/make_unique.h"

#include "core/file_sys/archive_systemsavedata.h"
#include "core/hle/service/fs/archive.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

static std::string GetSystemSaveDataPath(const std::string& mount_point, const Path& path) {
    std::vector<u8> vec_data = path.AsBinary();
    const u32* data = reinterpret_cast<const u32*>(vec_data.data());
    u32 save_low = data[1];
    u32 save_high = data[0];
    return Common::StringFromFormat("%s%08X/%08X/", mount_point.c_str(), save_low, save_high);
}

static std::string GetSystemSaveDataContainerPath(const std::string& mount_point) {
    return Common::StringFromFormat("%sdata/%s/sysdata/", mount_point.c_str(), SYSTEM_ID.c_str());
}

ArchiveFactory_SystemSaveData::ArchiveFactory_SystemSaveData(const std::string& nand_path)
        : base_path(GetSystemSaveDataContainerPath(nand_path)) {
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SystemSaveData::Open(const Path& path) {
    std::string fullpath = GetSystemSaveDataPath(base_path, path);
    if (!FileUtil::Exists(fullpath)) {
        // TODO(Subv): Check error code, this one is probably wrong
        return ResultCode(ErrorDescription::FS_NotFormatted, ErrorModule::FS,
            ErrorSummary::InvalidState, ErrorLevel::Status);
    }
    auto archive = Common::make_unique<DiskArchive>(fullpath);
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_SystemSaveData::Format(const Path& path) {
    std::string fullpath = GetSystemSaveDataPath(base_path, path);
    FileUtil::CreateFullPath(fullpath);
    return RESULT_SUCCESS;
}

} // namespace FileSys
