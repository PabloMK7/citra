// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"

#include "core/file_sys/file_romfs.h"
#include "core/file_sys/archive_romfs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

/**
 * Open the file
 * @return true if the file opened correctly
 */
bool File_RomFS::Open() {
    return true;
}

/**
 * Read data from the file
 * @param offset Offset in bytes to start reading data from
 * @param length Length in bytes of data to read from file
 * @param buffer Buffer to read data into
 * @return Number of bytes read
 */
size_t File_RomFS::Read(const u64 offset, const u32 length, u8* buffer) const {
    LOG_TRACE(Service_FS, "called offset=%llu, length=%d", offset, length);
    memcpy(buffer, &archive->raw_data[(u32)offset], length);
    return length;
}

/**
 * Write data to the file
 * @param offset Offset in bytes to start writing data to
 * @param length Length in bytes of data to write to file
 * @param flush The flush parameters (0 == do not flush)
 * @param buffer Buffer to read data from
 * @return Number of bytes written
 */
size_t File_RomFS::Write(const u64 offset, const u32 length, const u32 flush, const u8* buffer) const {
    LOG_WARNING(Service_FS, "Attempted to write to ROMFS.");
    return 0;
}

/**
 * Get the size of the file in bytes
 * @return Size of the file in bytes
 */
size_t File_RomFS::GetSize() const {
    return sizeof(u8) * archive->raw_data.size();
}

/**
 * Set the size of the file in bytes
 * @param size New size of the file
 * @return true if successful
 */
bool File_RomFS::SetSize(const u64 size) const {
    LOG_WARNING(Service_FS, "Attempted to set the size of ROMFS");
    return false;
}

/**
 * Close the file
 * @return true if the file closed correctly
 */
bool File_RomFS::Close() const {
    return false;
}

} // namespace FileSys
