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
#include "core/core.h"
#include "core/file_sys/archive_ncch.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/ivfc_archive.h"
#include "core/file_sys/ncch_container.h"
#include "core/file_sys/title_metadata.h"
#include "core/hle/service/fs/archive.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

static std::string GetNCCHContainerPath(const std::string& nand_directory) {
    return Common::StringFromFormat("%s%s/title/", nand_directory.c_str(), SYSTEM_ID);
}

static std::string GetNCCHPath(const std::string& mount_point, u32 high, u32 low) {
    u32 content_id = 0;

    // TODO(shinyquagsire23): Title database should be doing this path lookup
    std::string content_path =
        Common::StringFromFormat("%s%08x/%08x/content/", mount_point.c_str(), high, low);
    std::string tmd_path = content_path + "00000000.tmd";
    TitleMetadata tmd(tmd_path);
    if (tmd.Load() == Loader::ResultStatus::Success) {
        content_id = tmd.GetBootContentID();
    }

    return Common::StringFromFormat("%s%08x.app", content_path.c_str(), content_id);
}

ArchiveFactory_NCCH::ArchiveFactory_NCCH(const std::string& nand_directory)
    : mount_point(GetNCCHContainerPath(nand_directory)) {}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_NCCH::Open(const Path& path) {
    auto vec = path.AsBinary();
    const u32* data = reinterpret_cast<u32*>(vec.data());
    u32 high = data[1];
    u32 low = data[0];
    std::string file_path = GetNCCHPath(mount_point, high, low);

    std::shared_ptr<FileUtil::IOFile> romfs_file;
    u64 romfs_offset = 0;
    u64 romfs_size = 0;
    auto ncch_container = NCCHContainer(file_path);

    if (ncch_container.ReadRomFS(romfs_file, romfs_offset, romfs_size) !=
        Loader::ResultStatus::Success) {
        // High Title ID of the archive: The category (https://3dbrew.org/wiki/Title_list).
        constexpr u32 shared_data_archive = 0x0004009B;
        constexpr u32 system_data_archive = 0x000400DB;

        // Low Title IDs.
        constexpr u32 mii_data = 0x00010202;
        constexpr u32 region_manifest = 0x00010402;
        constexpr u32 ng_word_list = 0x00010302;

        LOG_DEBUG(Service_FS, "Full Path: %s. Category: 0x%X. Path: 0x%X.", path.DebugStr().c_str(),
                  high, low);

        if (high == shared_data_archive) {
            if (low == mii_data) {
                LOG_ERROR(Service_FS, "Failed to get a handle for shared data archive: Mii data. ");
                Core::System::GetInstance().SetStatus(Core::System::ResultStatus::ErrorSystemFiles,
                                                      "Mii data");
            } else if (low == region_manifest) {
                LOG_ERROR(Service_FS,
                          "Failed to get a handle for shared data archive: region manifest.");
                Core::System::GetInstance().SetStatus(Core::System::ResultStatus::ErrorSystemFiles,
                                                      "Region manifest");
            }
        } else if (high == system_data_archive) {
            if (low == ng_word_list) {
                LOG_ERROR(Service_FS,
                          "Failed to get a handle for system data archive: NG bad word list.");
                Core::System::GetInstance().SetStatus(Core::System::ResultStatus::ErrorSystemFiles,
                                                      "NG bad word list");
            }
        }
        return ERROR_NOT_FOUND;
    }

    auto archive = std::make_unique<IVFCArchive>(romfs_file, romfs_offset, romfs_size);
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_NCCH::Format(const Path& path,
                                       const FileSys::ArchiveFormatInfo& format_info) {
    LOG_ERROR(Service_FS, "Attempted to format a NCCH archive.");
    // TODO: Verify error code
    return ResultCode(ErrorDescription::NotAuthorized, ErrorModule::FS, ErrorSummary::NotSupported,
                      ErrorLevel::Permanent);
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_NCCH::GetFormatInfo(const Path& path) const {
    // TODO(Subv): Implement
    LOG_ERROR(Service_FS, "Unimplemented GetFormatInfo archive %s", GetName().c_str());
    return ResultCode(-1);
}

} // namespace FileSys
