// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"

#include "core/file_sys/file_romfs.h"
#include "core/file_sys/archive_romfs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

bool File_RomFS::Open() {
    return true;
}

size_t File_RomFS::Read(const u64 offset, const u32 length, u8* buffer) const {
    LOG_TRACE(Service_FS, "called offset=%llu, length=%d", offset, length);
    memcpy(buffer, &archive->raw_data[(u32)offset], length);
    return length;
}

size_t File_RomFS::Write(const u64 offset, const u32 length, const u32 flush, const u8* buffer) const {
    LOG_WARNING(Service_FS, "Attempted to write to ROMFS.");
    return 0;
}

size_t File_RomFS::GetSize() const {
    return sizeof(u8) * archive->raw_data.size();
}

bool File_RomFS::SetSize(const u64 size) const {
    LOG_WARNING(Service_FS, "Attempted to set the size of ROMFS");
    return false;
}

bool File_RomFS::Close() const {
    return false;
}

} // namespace FileSys
