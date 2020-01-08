// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "common/archives.h"
#include "common/file_util.h"
#include "core/file_sys/archive_sdmcwriteonly.h"
#include "core/file_sys/directory_backend.h"
#include "core/file_sys/errors.h"
#include "core/file_sys/file_backend.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

SERIALIZE_EXPORT_IMPL(FileSys::SDMCWriteOnlyArchive)
SERIALIZE_EXPORT_IMPL(FileSys::ArchiveFactory_SDMCWriteOnly)

namespace FileSys {

class SDMCWriteOnlyDelayGenerator : public DelayGenerator {
public:
    u64 GetReadDelayNs(std::size_t length) override {
        // This is the delay measured on O3DS and O2DS with
        // https://gist.github.com/B3n30/ac40eac20603f519ff106107f4ac9182
        // from the results the average of each length was taken.
        static constexpr u64 slope(183);
        static constexpr u64 offset(524879);
        static constexpr u64 minimum(631826);
        u64 IPCDelayNanoseconds = std::max<u64>(static_cast<u64>(length) * slope + offset, minimum);
        return IPCDelayNanoseconds;
    }

    u64 GetOpenDelayNs() override {
        // This is the delay measured on O3DS and O2DS with
        // https://gist.github.com/FearlessTobi/c37e143c314789251f98f2c45cd706d2
        // from the results the average of each length was taken.
        static constexpr u64 IPCDelayNanoseconds(269082);
        return IPCDelayNanoseconds;
    }

    SERIALIZE_DELAY_GENERATOR
};

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
    LOG_DEBUG(Service_FS, "Directory {} set as SDMCWriteOnly.", sdmc_directory);
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

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SDMCWriteOnly::Open(const Path& path,
                                                                              u64 program_id) {
    std::unique_ptr<DelayGenerator> delay_generator =
        std::make_unique<SDMCWriteOnlyDelayGenerator>();
    auto archive =
        std::make_unique<SDMCWriteOnlyArchive>(sdmc_directory, std::move(delay_generator));
    return MakeResult<std::unique_ptr<ArchiveBackend>>(std::move(archive));
}

ResultCode ArchiveFactory_SDMCWriteOnly::Format(const Path& path,
                                                const FileSys::ArchiveFormatInfo& format_info,
                                                u64 program_id) {
    // TODO(wwylele): hwtest this
    LOG_ERROR(Service_FS, "Attempted to format a SDMC write-only archive.");
    return ResultCode(-1);
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_SDMCWriteOnly::GetFormatInfo(const Path& path,
                                                                         u64 program_id) const {
    // TODO(Subv): Implement
    LOG_ERROR(Service_FS, "Unimplemented GetFormatInfo archive {}", GetName());
    return ResultCode(-1);
}

} // namespace FileSys

SERIALIZE_EXPORT_IMPL(FileSys::SDMCWriteOnlyDelayGenerator)
