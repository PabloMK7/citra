// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"

#include "core/file_sys/archive_romfs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

Archive_RomFS::Archive_RomFS(Loader::AppLoader& app_loader) {
    // Load the RomFS from the app
    if (Loader::ResultStatus::Success != app_loader.ReadRomFS(raw_data)) {
        WARN_LOG(FILESYS, "Unable to read RomFS!");
    }
}

Archive_RomFS::~Archive_RomFS() {
}

/**
 * Read data from the archive
 * @param offset Offset in bytes to start reading archive from
 * @param length Length in bytes to read data from archive
 * @param buffer Buffer to read data into
 * @return Number of bytes read
 */
size_t Archive_RomFS::Read(const u64 offset, const u32 length, u8* buffer) const {
    DEBUG_LOG(FILESYS, "called offset=%d, length=%d", offset, length);
    memcpy(buffer, &raw_data[(u32)offset], length);
    return length;
}

/**
 * Get the size of the archive in bytes
 * @return Size of the archive in bytes
 */
size_t Archive_RomFS::GetSize() const {
    ERROR_LOG(FILESYS, "(UNIMPLEMENTED)");
    return 0;
}

} // namespace FileSys
