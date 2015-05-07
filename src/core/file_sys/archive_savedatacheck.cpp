// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/make_unique.h"

#include "core/file_sys/archive_savedatacheck.h"
#include "core/hle/service/fs/archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

static std::string GetSaveDataCheckContainerPath(const std::string& nand_directory) {
    return Common::StringFromFormat("%s%s/title/", nand_directory.c_str(), SYSTEM_ID.c_str());
}

static std::string GetSaveDataCheckPath(const std::string& mount_point, u32 high, u32 low) {
    return Common::StringFromFormat("%s%08x/%08x/content/00000000.app.romfs",
            mount_point.c_str(), high, low);
}

ArchiveFactory_SaveDataCheck::ArchiveFactory_SaveDataCheck(const std::string& nand_directory) :
        mount_point(GetSaveDataCheckContainerPath(nand_directory)) {
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SaveDataCheck::Open(const Path& path) {
    auto vec = path.AsBinary();
    const u32* data = reinterpret_cast<u32*>(vec.data());
    std::string file_path = GetSaveDataCheckPath(mount_point, data[1], data[0]);
    FileUtil::IOFile file(file_path, "rb");

    if (!file.IsOpen()) {
        return ResultCode(-1); // TODO(Subv): Find the right error code
    }
    auto size = file.GetSize();
    auto raw_data = std::make_shared<std::vector<u8>>(size);
    file.ReadBytes(raw_data->data(), size);
    file.Close();

    auto archive = Common::make_unique<IVFCArchive>(std::move(raw_data));
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_SaveDataCheck::Format(const Path& path) {
    LOG_ERROR(Service_FS, "Attempted to format a SaveDataCheck archive.");
    // TODO: Verify error code
    return ResultCode(ErrorDescription::NotAuthorized, ErrorModule::FS,
        ErrorSummary::NotSupported, ErrorLevel::Permanent);
}

} // namespace FileSys
