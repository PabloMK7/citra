// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstring>
#include <memory>
#include <vector>
#include <fmt/format.h>
#include "common/common_types.h"
#include "common/file_util.h"
#include "core/file_sys/archive_systemsavedata.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/savedata_archive.h"
#include "core/hle/service/fs/archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

std::string GetSystemSaveDataPath(const std::string& mount_point, const Path& path) {
    const std::vector<u8> vec_data = path.AsBinary();
    u32 save_low;
    u32 save_high;
    std::memcpy(&save_low, &vec_data[4], sizeof(u32));
    std::memcpy(&save_high, &vec_data[0], sizeof(u32));
    return fmt::format("{}{:08X}/{:08X}/", mount_point, save_low, save_high);
}

std::string GetSystemSaveDataContainerPath(const std::string& mount_point) {
    return fmt::format("{}data/{}/sysdata/", mount_point, SYSTEM_ID);
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

    return {binary_path};
}

ArchiveFactory_SystemSaveData::ArchiveFactory_SystemSaveData(const std::string& nand_path)
    : base_path(GetSystemSaveDataContainerPath(nand_path)) {}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SystemSaveData::Open(const Path& path) {
    std::string fullpath = GetSystemSaveDataPath(base_path, path);
    if (!FileUtil::Exists(fullpath)) {
        // TODO(Subv): Check error code, this one is probably wrong
        return ERR_NOT_FORMATTED;
    }
    auto archive = std::make_unique<SaveDataArchive>(fullpath);
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_SystemSaveData::Format(const Path& path,
                                                 const FileSys::ArchiveFormatInfo& format_info) {
    std::string fullpath = GetSystemSaveDataPath(base_path, path);
    FileUtil::DeleteDirRecursively(fullpath);
    FileUtil::CreateFullPath(fullpath);
    return RESULT_SUCCESS;
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_SystemSaveData::GetFormatInfo(const Path& path) const {
    // TODO(Subv): Implement
    LOG_ERROR(Service_FS, "Unimplemented GetFormatInfo archive {}", GetName());
    return ResultCode(-1);
}

} // namespace FileSys
