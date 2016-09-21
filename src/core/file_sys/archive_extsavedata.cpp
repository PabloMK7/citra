// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <vector>
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/file_sys/archive_extsavedata.h"
#include "core/file_sys/disk_archive.h"
#include "core/hle/service/fs/archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

std::string GetExtSaveDataPath(const std::string& mount_point, const Path& path) {
    std::vector<u8> vec_data = path.AsBinary();
    const u32* data = reinterpret_cast<const u32*>(vec_data.data());
    u32 save_low = data[1];
    u32 save_high = data[2];
    return Common::StringFromFormat("%s%08X/%08X/", mount_point.c_str(), save_high, save_low);
}

std::string GetExtDataContainerPath(const std::string& mount_point, bool shared) {
    if (shared)
        return Common::StringFromFormat("%sdata/%s/extdata/", mount_point.c_str(),
                                        SYSTEM_ID.c_str());

    return Common::StringFromFormat("%sNintendo 3DS/%s/%s/extdata/", mount_point.c_str(),
                                    SYSTEM_ID.c_str(), SDCARD_ID.c_str());
}

Path ConstructExtDataBinaryPath(u32 media_type, u32 high, u32 low) {
    std::vector<u8> binary_path;
    binary_path.reserve(12);

    // Append each word byte by byte

    // The first word is the media type
    for (unsigned i = 0; i < 4; ++i)
        binary_path.push_back((media_type >> (8 * i)) & 0xFF);

    // Next is the low word
    for (unsigned i = 0; i < 4; ++i)
        binary_path.push_back((low >> (8 * i)) & 0xFF);

    // Next is the high word
    for (unsigned i = 0; i < 4; ++i)
        binary_path.push_back((high >> (8 * i)) & 0xFF);

    return {binary_path};
}

ArchiveFactory_ExtSaveData::ArchiveFactory_ExtSaveData(const std::string& mount_location,
                                                       bool shared)
    : shared(shared), mount_point(GetExtDataContainerPath(mount_location, shared)) {
    LOG_INFO(Service_FS, "Directory %s set as base for ExtSaveData.", mount_point.c_str());
}

bool ArchiveFactory_ExtSaveData::Initialize() {
    if (!FileUtil::CreateFullPath(mount_point)) {
        LOG_ERROR(Service_FS, "Unable to create ExtSaveData base path.");
        return false;
    }

    return true;
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_ExtSaveData::Open(const Path& path) {
    std::string fullpath = GetExtSaveDataPath(mount_point, path) + "user/";
    if (!FileUtil::Exists(fullpath)) {
        // TODO(Subv): Verify the archive behavior of SharedExtSaveData compared to ExtSaveData.
        // ExtSaveData seems to return FS_NotFound (120) when the archive doesn't exist.
        if (!shared) {
            return ResultCode(ErrorDescription::FS_NotFound, ErrorModule::FS,
                              ErrorSummary::InvalidState, ErrorLevel::Status);
        } else {
            return ResultCode(ErrorDescription::FS_NotFormatted, ErrorModule::FS,
                              ErrorSummary::InvalidState, ErrorLevel::Status);
        }
    }
    auto archive = std::make_unique<DiskArchive>(fullpath);
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_ExtSaveData::Format(const Path& path,
                                              const FileSys::ArchiveFormatInfo& format_info) {
    // These folders are always created with the ExtSaveData
    std::string user_path = GetExtSaveDataPath(mount_point, path) + "user/";
    std::string boss_path = GetExtSaveDataPath(mount_point, path) + "boss/";
    FileUtil::CreateFullPath(user_path);
    FileUtil::CreateFullPath(boss_path);

    // Write the format metadata
    std::string metadata_path = GetExtSaveDataPath(mount_point, path) + "metadata";
    FileUtil::IOFile file(metadata_path, "wb");

    if (!file.IsOpen()) {
        // TODO(Subv): Find the correct error code
        return ResultCode(-1);
    }

    file.WriteBytes(&format_info, sizeof(format_info));
    return RESULT_SUCCESS;
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_ExtSaveData::GetFormatInfo(const Path& path) const {
    std::string metadata_path = GetExtSaveDataPath(mount_point, path) + "metadata";
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

void ArchiveFactory_ExtSaveData::WriteIcon(const Path& path, const u8* icon_data,
                                           size_t icon_size) {
    std::string game_path = FileSys::GetExtSaveDataPath(GetMountPoint(), path);
    FileUtil::IOFile icon_file(game_path + "icon", "wb");
    icon_file.WriteBytes(icon_data, icon_size);
}

} // namespace FileSys
