// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/disk_archive.h"
#include "core/settings.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

std::unique_ptr<FileBackend> DiskArchive::OpenFile(const Path& path, const Mode mode) const {
    LOG_DEBUG(Service_FS, "called path=%s mode=%01X", path.DebugStr().c_str(), mode.hex);
    DiskFile* file = new DiskFile(this, path, mode);
    if (!file->Open())
        return nullptr;
    return std::unique_ptr<FileBackend>(file);
}

bool DiskArchive::DeleteFile(const FileSys::Path& path) const {
    return FileUtil::Delete(GetMountPoint() + path.AsString());
}

bool DiskArchive::RenameFile(const FileSys::Path& src_path, const FileSys::Path& dest_path) const {
    return FileUtil::Rename(GetMountPoint() + src_path.AsString(), GetMountPoint() + dest_path.AsString());
}

bool DiskArchive::DeleteDirectory(const FileSys::Path& path) const {
    return FileUtil::DeleteDir(GetMountPoint() + path.AsString());
}

bool DiskArchive::CreateDirectory(const Path& path) const {
    return FileUtil::CreateDir(GetMountPoint() + path.AsString());
}

bool DiskArchive::RenameDirectory(const FileSys::Path& src_path, const FileSys::Path& dest_path) const {
    return FileUtil::Rename(GetMountPoint() + src_path.AsString(), GetMountPoint() + dest_path.AsString());
}

std::unique_ptr<DirectoryBackend> DiskArchive::OpenDirectory(const Path& path) const {
    LOG_DEBUG(Service_FS, "called path=%s", path.DebugStr().c_str());
    DiskDirectory* directory = new DiskDirectory(this, path);
    if (!directory->Open())
        return nullptr;
    return std::unique_ptr<DirectoryBackend>(directory);
}

////////////////////////////////////////////////////////////////////////////////////////////////////

DiskFile::DiskFile(const DiskArchive* archive, const Path& path, const Mode mode) {
    // TODO(Link Mauve): normalize path into an absolute path without "..", it can currently bypass
    // the root directory we set while opening the archive.
    // For example, opening /../../etc/passwd can give the emulated program your users list.
    this->path = archive->GetMountPoint() + path.AsString();
    this->mode.hex = mode.hex;
    this->archive = archive;
}

bool DiskFile::Open() {
    if (!mode.create_flag && !FileUtil::Exists(path)) {
        LOG_ERROR(Service_FS, "Non-existing file %s can’t be open without mode create.", path.c_str());
        return false;
    }

    std::string mode_string;
    if (mode.create_flag)
        mode_string = "w+";
    else if (mode.write_flag)
        mode_string = "r+"; // Files opened with Write access can be read from
    else if (mode.read_flag)
        mode_string = "r";

    // Open the file in binary mode, to avoid problems with CR/LF on Windows systems
    mode_string += "b";

    file = new FileUtil::IOFile(path, mode_string.c_str());
    return true;
}

size_t DiskFile::Read(const u64 offset, const u32 length, u8* buffer) const {
    file->Seek(offset, SEEK_SET);
    return file->ReadBytes(buffer, length);
}

size_t DiskFile::Write(const u64 offset, const u32 length, const u32 flush, const u8* buffer) const {
    file->Seek(offset, SEEK_SET);
    size_t written = file->WriteBytes(buffer, length);
    if (flush)
        file->Flush();
    return written;
}

size_t DiskFile::GetSize() const {
    return static_cast<size_t>(file->GetSize());
}

bool DiskFile::SetSize(const u64 size) const {
    file->Resize(size);
    file->Flush();
    return true;
}

bool DiskFile::Close() const {
    return file->Close();
}

////////////////////////////////////////////////////////////////////////////////////////////////////

DiskDirectory::DiskDirectory(const DiskArchive* archive, const Path& path) {
    // TODO(Link Mauve): normalize path into an absolute path without "..", it can currently bypass
    // the root directory we set while opening the archive.
    // For example, opening /../../usr/bin can give the emulated program your installed programs.
    this->path = archive->GetMountPoint() + path.AsString();
    this->archive = archive;
}

bool DiskDirectory::Open() {
    if (!FileUtil::IsDirectory(path))
        return false;
    FileUtil::ScanDirectoryTree(path, directory);
    children_iterator = directory.children.begin();
    return true;
}

u32 DiskDirectory::Read(const u32 count, Entry* entries) {
    u32 entries_read = 0;

    while (entries_read < count && children_iterator != directory.children.cend()) {
        const FileUtil::FSTEntry& file = *children_iterator;
        const std::string& filename = file.virtualName;
        Entry& entry = entries[entries_read];

        LOG_TRACE(Service_FS, "File %s: size=%llu dir=%d", filename.c_str(), file.size, file.isDirectory);

        // TODO(Link Mauve): use a proper conversion to UTF-16.
        for (size_t j = 0; j < FILENAME_LENGTH; ++j) {
            entry.filename[j] = filename[j];
            if (!filename[j])
                break;
        }

        FileUtil::SplitFilename83(filename, entry.short_name, entry.extension);

        entry.is_directory = file.isDirectory;
        entry.is_hidden = (filename[0] == '.');
        entry.is_read_only = 0;
        entry.file_size = file.size;

        // We emulate a SD card where the archive bit has never been cleared, as it would be on
        // most user SD cards.
        // Some homebrews (blargSNES for instance) are known to mistakenly use the archive bit as a
        // file bit.
        entry.is_archive = !file.isDirectory;

        ++entries_read;
        ++children_iterator;
    }
    return entries_read;
}

} // namespace FileSys
