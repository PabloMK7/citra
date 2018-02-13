// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cstdio>
#include <memory>
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "core/file_sys/disk_archive.h"
#include "core/file_sys/errors.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

ResultVal<size_t> DiskFile::Read(const u64 offset, const size_t length, u8* buffer) const {
    if (!mode.read_flag)
        return ERROR_INVALID_OPEN_FLAGS;

    file->Seek(offset, SEEK_SET);
    return MakeResult<size_t>(file->ReadBytes(buffer, length));
}

ResultVal<size_t> DiskFile::Write(const u64 offset, const size_t length, const bool flush,
                                  const u8* buffer) {
    if (!mode.write_flag)
        return ERROR_INVALID_OPEN_FLAGS;

    file->Seek(offset, SEEK_SET);
    size_t written = file->WriteBytes(buffer, length);
    if (flush)
        file->Flush();
    return MakeResult<size_t>(written);
}

u64 DiskFile::GetReadDelayNs(size_t length) const {
    // TODO(B3N30): figure out the time a 3ds needs for those write
    // for that backend.
    // For now take the results from the romfs test.
    // The delay was measured on O3DS and O2DS with
    // https://gist.github.com/B3n30/ac40eac20603f519ff106107f4ac9182
    // from the results the average of each length was taken.
    static constexpr u64 slope(183);
    static constexpr u64 offset(524879);
    static constexpr u64 minimum(631826);
    u64 IPCDelayNanoseconds = std::max<u64>(static_cast<u64>(length) * slope + offset, minimum);
    return IPCDelayNanoseconds;
}

u64 DiskFile::GetSize() const {
    return file->GetSize();
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

DiskDirectory::DiskDirectory(const std::string& path) : directory() {
    unsigned size = FileUtil::ScanDirectoryTree(path, directory);
    directory.size = size;
    directory.isDirectory = true;
    children_iterator = directory.children.begin();
}

u32 DiskDirectory::Read(const u32 count, Entry* entries) {
    u32 entries_read = 0;

    while (entries_read < count && children_iterator != directory.children.cend()) {
        const FileUtil::FSTEntry& file = *children_iterator;
        const std::string& filename = file.virtualName;
        Entry& entry = entries[entries_read];

        LOG_TRACE(Service_FS, "File %s: size=%llu dir=%d", filename.c_str(), file.size,
                  file.isDirectory);

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
