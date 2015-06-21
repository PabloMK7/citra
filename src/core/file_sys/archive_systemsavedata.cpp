// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <vector>

#include "common/common_types.h"
#include "common/file_util.h"
#include "common/make_unique.h"
#include "common/string_util.h"

#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/disk_archive.h"
#include "core/hle/service/fs/archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

std::string GetSystemSaveDataPath(const std::string& mount_point, const Path& path) {
    std::vector<u8> vec_data = path.AsBinary();
    const u32* data = reinterpret_cast<const u32*>(vec_data.data());
    u32 save_low = data[1];
    u32 save_high = data[0];
    return Common::StringFromFormat("%s%08X/%08X/", mount_point.c_str(), save_low, save_high);
}

std::string GetSystemSaveDataContainerPath(const std::string& mount_point) {
    return Common::StringFromFormat("%sdata/%s/sysdata/", mount_point.c_str(), SYSTEM_ID.c_str());
}

Path ConstructSystemSaveDataBinaryPath(u32 high, u32 low) {
    std::vector<u8> binary_path;
    binary_path.reserve(8);

    // Append each word byte by byte

    // First is the high word
    for (unsigned i = 0; i < 4; ++i)
        binary_path.push_back((high >> (8 * i)) & 0xFF);

    // Next is the low word
    for (unsigned i = 0; i < 4; ++i)
        binary_path.push_back((low >> (8 * i)) & 0xFF);

    return { binary_path };
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
    FileUtil::DeleteDirRecursively(fullpath);
    FileUtil::CreateFullPath(fullpath);
    return RESULT_SUCCESS;
}

} // namespace FileSys
