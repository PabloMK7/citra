// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/archive_sdmc.h"
#include "core/file_sys/directory_sdmc.h"
#include "core/file_sys/file_sdmc.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

Archive_SDMC::Archive_SDMC(const std::string& mount_point) {
    this->mount_point = mount_point;
    DEBUG_LOG(FILESYS, "Directory %s set as SDMC.", mount_point.c_str());
}

Archive_SDMC::~Archive_SDMC() {
}

/**
 * Initialize the archive.
 * @return true if it initialized successfully
 */
bool Archive_SDMC::Initialize() {
    if (!Settings::values.use_virtual_sd) {
        WARN_LOG(FILESYS, "SDMC disabled by config.");
        return false;
    }

    if (!FileUtil::CreateFullPath(mount_point)) {
        WARN_LOG(FILESYS, "Unable to create SDMC path.");
        return false;
    }

    return true;
}

/**
 * Open a file specified by its path, using the specified mode
 * @param path Path relative to the archive
 * @param mode Mode to open the file with
 * @return Opened file, or nullptr
 */
std::unique_ptr<File> Archive_SDMC::OpenFile(const Path& path, const Mode mode) const {
    DEBUG_LOG(FILESYS, "called path=%s mode=%d", path.DebugStr().c_str(), mode);
    File_SDMC* file = new File_SDMC(this, path, mode);
    if (!file->Open())
        return nullptr;
    return std::unique_ptr<File>(file);
}

/**
 * Delete a file specified by its path
 * @param path Path relative to the archive
 * @return Whether the file could be deleted
 */
bool Archive_SDMC::DeleteFile(const FileSys::Path& path) const {
    return FileUtil::Delete(GetMountPoint() + path.AsString());
}

bool Archive_SDMC::RenameFile(const FileSys::Path& src_path, const FileSys::Path& dest_path) const {
    return FileUtil::Rename(GetMountPoint() + src_path.AsString(), GetMountPoint() + dest_path.AsString());
}

/**
 * Delete a directory specified by its path
 * @param path Path relative to the archive
 * @return Whether the directory could be deleted
 */
bool Archive_SDMC::DeleteDirectory(const FileSys::Path& path) const {
    return FileUtil::DeleteDir(GetMountPoint() + path.AsString());
}

/**
 * Create a directory specified by its path
 * @param path Path relative to the archive
 * @return Whether the directory could be created
 */
bool Archive_SDMC::CreateDirectory(const Path& path) const {
    return FileUtil::CreateDir(GetMountPoint() + path.AsString());
}

bool Archive_SDMC::RenameDirectory(const FileSys::Path& src_path, const FileSys::Path& dest_path) const {
    return FileUtil::Rename(GetMountPoint() + src_path.AsString(), GetMountPoint() + dest_path.AsString());
}

/**
 * Open a directory specified by its path
 * @param path Path relative to the archive
 * @return Opened directory, or nullptr
 */
std::unique_ptr<Directory> Archive_SDMC::OpenDirectory(const Path& path) const {
    DEBUG_LOG(FILESYS, "called path=%s", path.DebugStr().c_str());
    Directory_SDMC* directory = new Directory_SDMC(this, path);
    return std::unique_ptr<Directory>(directory);
}

/**
 * Read data from the archive
 * @param offset Offset in bytes to start reading archive from
 * @param length Length in bytes to read data from archive
 * @param buffer Buffer to read data into
 * @return Number of bytes read
 */
size_t Archive_SDMC::Read(const u64 offset, const u32 length, u8* buffer) const {
    ERROR_LOG(FILESYS, "(UNIMPLEMENTED)");
    return -1;
}

/**
 * Write data to the archive
 * @param offset Offset in bytes to start writing data to
 * @param length Length in bytes of data to write to archive
 * @param buffer Buffer to write data from
 * @param flush  The flush parameters (0 == do not flush)
 * @return Number of bytes written
 */
size_t Archive_SDMC::Write(const u64 offset, const u32 length, const u32 flush, u8* buffer) {
    ERROR_LOG(FILESYS, "(UNIMPLEMENTED)");
    return -1;
}

/**
 * Get the size of the archive in bytes
 * @return Size of the archive in bytes
 */
size_t Archive_SDMC::GetSize() const {
    ERROR_LOG(FILESYS, "(UNIMPLEMENTED)");
    return 0;
}

/**
 * Set the size of the archive in bytes
 */
void Archive_SDMC::SetSize(const u64 size) {
    ERROR_LOG(FILESYS, "(UNIMPLEMENTED)");
}

/**
 * Getter for the path used for this Archive
 * @return Mount point of that passthrough archive
 */
std::string Archive_SDMC::GetMountPoint() const {
    return mount_point;
}

} // namespace FileSys
