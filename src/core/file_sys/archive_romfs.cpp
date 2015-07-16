// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "common/make_unique.h"

#include "core/file_sys/archive_romfs.h"
#include "core/file_sys/ivfc_archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

ArchiveFactory_RomFS::ArchiveFactory_RomFS(Loader::AppLoader& app_loader) {
    // Load the RomFS from the app
    if (Loader::ResultStatus::Success != app_loader.ReadRomFS(romfs_file, data_offset, data_size)) {
        LOG_ERROR(Service_FS, "Unable to read RomFS!");
    }
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_RomFS::Open(const Path& path) {
    auto archive = Common::make_unique<IVFCArchive>(romfs_file, data_offset, data_size);
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_RomFS::Format(const Path& path) {
    LOG_ERROR(Service_FS, "Attempted to format a RomFS archive.");
    // TODO: Verify error code
    return ResultCode(ErrorDescription::NotAuthorized, ErrorModule::FS,
            ErrorSummary::NotSupported, ErrorLevel::Permanent);
}

} // namespace FileSys
