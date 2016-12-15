// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include "core/file_sys/archive_backend.h"
#include "core/hle/result.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/// A common source of SD save data archive
class ArchiveSource_SDSaveData {
public:
    explicit ArchiveSource_SDSaveData(const std::string& mount_point);

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(u64 program_id);
    ResultCode Format(u64 program_id, const FileSys::ArchiveFormatInfo& format_info);
    ResultVal<ArchiveFormatInfo> GetFormatInfo(u64 program_id) const;

    static std::string GetSaveDataPathFor(const std::string& mount_point, u64 program_id);

private:
    std::string mount_point;
};

} // namespace FileSys
