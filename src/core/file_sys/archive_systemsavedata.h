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

/// File system interface to the SystemSaveData archive
/// TODO(Subv): This archive should point to a location in the NAND, 
/// specifically nand:/data/<ID0>/sysdata/<SaveID-Low>/<SaveID-High>
class Archive_SystemSaveData final : public DiskArchive {
public:
    Archive_SystemSaveData(const std::string& mount_point, u64 save_id);

    /**
     * Initialize the archive.
     * @return true if it initialized successfully
     */
    bool Initialize();

    std::string GetName() const override { return "SystemSaveData"; }
};

} // namespace FileSys
