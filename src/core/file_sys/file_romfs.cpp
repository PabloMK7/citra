// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"

#include "core/file_sys/file_romfs.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

File_RomFS::File_RomFS() {
}

File_RomFS::~File_RomFS() {
}

/**
 * Open the file
 * @return true if the file opened correctly
 */
bool File_RomFS::Open() {
    return false;
}

/**
 * Read data from the file
 * @param offset Offset in bytes to start reading data from
 * @param length Length in bytes of data to read from file
 * @param buffer Buffer to read data into
 * @return Number of bytes read
 */
size_t File_RomFS::Read(const u64 offset, const u32 length, u8* buffer) const {
    return -1;
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
    return -1;
}

/**
 * Get the size of the file in bytes
 * @return Size of the file in bytes
 */
size_t File_RomFS::GetSize() const {
    return -1;
}

/**
 * Set the size of the file in bytes
 * @param size New size of the file
 * @return true if successful
 */
bool File_RomFS::SetSize(const u64 size) const {
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
