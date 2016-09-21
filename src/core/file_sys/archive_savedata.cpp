// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/file_sys/archive_savedata.h"
#include "core/file_sys/disk_archive.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/fs/archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

static std::string GetSaveDataContainerPath(const std::string& sdmc_directory) {
    return Common::StringFromFormat("%sNintendo 3DS/%s/%s/title/", sdmc_directory.c_str(),
                                    SYSTEM_ID.c_str(), SDCARD_ID.c_str());
}

static std::string GetSaveDataPath(const std::string& mount_location, u64 program_id) {
    u32 high = (u32)(program_id >> 32);
    u32 low = (u32)(program_id & 0xFFFFFFFF);
    return Common::StringFromFormat("%s%08x/%08x/data/00000001/", mount_location.c_str(), high,
                                    low);
}

static std::string GetSaveDataMetadataPath(const std::string& mount_location, u64 program_id) {
    u32 high = (u32)(program_id >> 32);
    u32 low = (u32)(program_id & 0xFFFFFFFF);
    return Common::StringFromFormat("%s%08x/%08x/data/00000001.metadata", mount_location.c_str(),
                                    high, low);
}

ArchiveFactory_SaveData::ArchiveFactory_SaveData(const std::string& sdmc_directory)
    : mount_point(GetSaveDataContainerPath(sdmc_directory)) {
    LOG_INFO(Service_FS, "Directory %s set as SaveData.", this->mount_point.c_str());
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SaveData::Open(const Path& path) {
    std::string concrete_mount_point =
        GetSaveDataPath(mount_point, Kernel::g_current_process->codeset->program_id);
    if (!FileUtil::Exists(concrete_mount_point)) {
        // When a SaveData archive is created for the first time, it is not yet formatted and the
        // save file/directory structure expected by the game has not yet been initialized.
        // Returning the NotFormatted error code will signal the game to provision the SaveData
        // archive with the files and folders that it expects.
        return ResultCode(ErrorDescription::FS_NotFormatted, ErrorModule::FS,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    }

    auto archive = std::make_unique<DiskArchive>(std::move(concrete_mount_point));
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_SaveData::Format(const Path& path,
                                           const FileSys::ArchiveFormatInfo& format_info) {
    std::string concrete_mount_point =
        GetSaveDataPath(mount_point, Kernel::g_current_process->codeset->program_id);
    FileUtil::DeleteDirRecursively(concrete_mount_point);
    FileUtil::CreateFullPath(concrete_mount_point);

    // Write the format metadata
    std::string metadata_path =
        GetSaveDataMetadataPath(mount_point, Kernel::g_current_process->codeset->program_id);
    FileUtil::IOFile file(metadata_path, "wb");

    if (file.IsOpen()) {
        file.WriteBytes(&format_info, sizeof(format_info));
        return RESULT_SUCCESS;
    }
    return RESULT_SUCCESS;
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_SaveData::GetFormatInfo(const Path& path) const {
    std::string metadata_path =
        GetSaveDataMetadataPath(mount_point, Kernel::g_current_process->codeset->program_id);
    FileUtil::IOFile file(metadata_path, "rb");

    if (!file.IsOpen()) {
        LOG_ERROR(Service_FS, "Could not open metadata information for archive");
        // TODO(Subv): Verify error code
        return ResultCode(ErrorDescription::FS_NotFormatted, ErrorModule::FS,
                          ErrorSummary::InvalidState, ErrorLevel::Status);
    }

    ArchiveFormatInfo info = {};
    file.ReadBytes(&info, sizeof(info));
    return MakeResult<ArchiveFormatInfo>(info);
}

} // namespace FileSys
