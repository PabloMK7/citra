// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"

#include "core/file_sys/directory_romfs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

Directory_RomFS::Directory_RomFS() {
}

Directory_RomFS::~Directory_RomFS() {
}

/**
 * List files contained in the directory
 * @param count Number of entries to return at once in entries
 * @param entries Buffer to read data into
 * @return Number of entries listed
 */
u32 Directory_RomFS::Read(const u32 count, Entry* entries) {
    return 0;
}

/**
 * Close the directory
 * @return true if the directory closed correctly
 */
bool Directory_RomFS::Close() const {
    return false;
}

} // namespace FileSys
