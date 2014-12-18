// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
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

bool Directory_RomFS::Open() {
    return false;
}

u32 Directory_RomFS::Read(const u32 count, Entry* entries) {
    return 0;
}

bool Directory_RomFS::Close() const {
    return false;
}

} // namespace FileSys
