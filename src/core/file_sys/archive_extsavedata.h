// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/file_sys/disk_archive.h"
#include "core/loader/loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/// File system interface to the ExtSaveData archive
class Archive_ExtSaveData final : public DiskArchive {
public:
    Archive_ExtSaveData(const std::string& mount_point);

    /**
     * Initialize the archive.
     * @return true if it initialized successfully
     */
    bool Initialize();

    ResultCode Open(const Path& path) override;
    ResultCode Format(const Path& path) const override;
    std::string GetName() const override { return "ExtSaveData"; }

    const std::string& GetMountPoint() const override {
        return concrete_mount_point;
    }

protected:
    /**
     * This holds the full directory path for this archive, it is only set after a successful call to Open, 
     * this is formed as <base extsavedatapath>/<type>/<high>/<low>. 
     * See GetExtSaveDataPath for the code that extracts this data from an archive path.
     */
    std::string concrete_mount_point;
};

} // namespace FileSys
