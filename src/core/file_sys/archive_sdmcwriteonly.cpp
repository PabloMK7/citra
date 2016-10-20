// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "common/file_util.h"
#include "core/file_sys/archive_sdmcwriteonly.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

ResultVal<std::unique_ptr<FileBackend>> SDMCWriteOnlyArchive::OpenFile(const Path& path,
                                                                       const Mode& mode) const {
    if (mode.read_flag) {
        LOG_ERROR(Service_FS, "Read flag is not supported");
        return ERROR_INVALID_READ_FLAG;
    }
    return SDMCArchive::OpenFileBase(path, mode);
}

ResultVal<std::unique_ptr<DirectoryBackend>> SDMCWriteOnlyArchive::OpenDirectory(
    const Path& path) const {
    LOG_ERROR(Service_FS, "Not supported");
    return ERROR_UNSUPPORTED_OPEN_FLAGS;
}

ArchiveFactory_SDMCWriteOnly::ArchiveFactory_SDMCWriteOnly(const std::string& mount_point)
    : sdmc_directory(mount_point) {
    LOG_INFO(Service_FS, "Directory %s set as SDMCWriteOnly.", sdmc_directory.c_str());
}

bool ArchiveFactory_SDMCWriteOnly::Initialize() {
    if (!Settings::values.use_virtual_sd) {
        LOG_WARNING(Service_FS, "SDMC disabled by config.");
        return false;
    }

    if (!FileUtil::CreateFullPath(sdmc_directory)) {
        LOG_ERROR(Service_FS, "Unable to create SDMC path.");
        return false;
    }

    return true;
}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SDMCWriteOnly::Open(const Path& path) {
    auto archive = std::make_unique<SDMCWriteOnlyArchive>(sdmc_directory);
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_SDMCWriteOnly::Format(const Path& path,
                                                const FileSys::ArchiveFormatInfo& format_info) {
    // TODO(wwylele): hwtest this
    LOG_ERROR(Service_FS, "Attempted to format a SDMC write-only archive.");
    return ResultCode(-1);
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_SDMCWriteOnly::GetFormatInfo(const Path& path) const {
    // TODO(Subv): Implement
    LOG_ERROR(Service_FS, "Unimplemented GetFormatInfo archive %s", GetName().c_str());
    return ResultCode(-1);
}

} // namespace FileSys
