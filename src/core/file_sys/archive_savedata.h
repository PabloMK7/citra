// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/file_sys/disk_archive.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/// File system interface to the SaveData archive
class Archive_SaveData final : public DiskArchive {
public:
    Archive_SaveData(const std::string& mount_point, u64 program_id);

    /**
     * Initialize the archive.
     * @return CreateSaveDataResult AlreadyExists if the SaveData folder already exists,
     * Success if it was created properly and Failure if there was any error
     */
    bool Initialize();

    std::string GetName() const override { return "SaveData"; }
};

} // namespace FileSys
