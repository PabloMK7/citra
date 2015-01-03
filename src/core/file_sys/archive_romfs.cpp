// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/common_types.h"
#include "common/file_util.h"
#include "common/make_unique.h"

#include "core/file_sys/archive_romfs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

Archive_RomFS::Archive_RomFS(const Loader::AppLoader& app_loader) {
    // Load the RomFS from the app
    if (Loader::ResultStatus::Success != app_loader.ReadRomFS(raw_data)) {
        LOG_ERROR(Service_FS, "Unable to read RomFS!");
    }
}

} // namespace FileSys
