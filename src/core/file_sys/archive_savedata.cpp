// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <utility>
#include "core/core.h"
#include "core/file_sys/archive_savedata.h"
#include "core/hle/kernel/process.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

ArchiveFactory_SaveData::ArchiveFactory_SaveData(
    std::shared_ptr<ArchiveSource_SDSaveData> sd_savedata)
    : sd_savedata_source(std::move(sd_savedata)) {}

ResultVal<std::unique_ptr<ArchiveBackend>> ArchiveFactory_SaveData::Open(const Path& path) {
    return sd_savedata_source->Open(
        Core::System::GetInstance().Kernel().GetCurrentProcess()->codeset->program_id);
}

ResultCode ArchiveFactory_SaveData::Format(const Path& path,
                                           const FileSys::ArchiveFormatInfo& format_info) {
    return sd_savedata_source->Format(
        Core::System::GetInstance().Kernel().GetCurrentProcess()->codeset->program_id, format_info);
}

ResultVal<ArchiveFormatInfo> ArchiveFactory_SaveData::GetFormatInfo(const Path& path) const {
    return sd_savedata_source->GetFormatInfo(
        Core::System::GetInstance().Kernel().GetCurrentProcess()->codeset->program_id);
}

} // namespace FileSys
