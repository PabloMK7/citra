// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/file_sys/archive_source_sd_savedata.h"

namespace FileSys {

/// File system interface to the SaveData archive
class ArchiveFactory_SaveData final : public ArchiveFactory {
public:
    explicit ArchiveFactory_SaveData(std::shared_ptr<ArchiveSource_SDSaveData> sd_savedata_source);

    std::string GetName() const override {
        return "SaveData";
    }

    ResultVal<std::unique_ptr<ArchiveBackend>> Open(const Path& path, u64 program_id) override;
    Result Format(const Path& path, const FileSys::ArchiveFormatInfo& format_info,
                  u64 program_id) override;

    ResultVal<ArchiveFormatInfo> GetFormatInfo(const Path& path, u64 program_id) const override;

private:
    std::shared_ptr<ArchiveSource_SDSaveData> sd_savedata_source;
};

} // namespace FileSys
