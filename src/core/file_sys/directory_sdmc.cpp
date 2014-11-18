// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <sys/stat.h>

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/file_sys/directory_sdmc.h"
#include "core/file_sys/archive_sdmc.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// FileSys namespace

namespace FileSys {

Directory_SDMC::Directory_SDMC(const Archive_SDMC* archive, const Path& path) {
    // TODO(Link Mauve): normalize path into an absolute path without "..", it can currently bypass
    // the root directory we set while opening the archive.
    // For example, opening /../../usr/bin can give the emulated program your installed programs.
    std::string absolute_path = archive->GetMountPoint() + path.AsString();
    FileUtil::ScanDirectoryTree(absolute_path, directory);
    children_iterator = directory.children.begin();
}

Directory_SDMC::~Directory_SDMC() {
    Close();
}

/**
 * List files contained in the directory
 * @param count Number of entries to return at once in entries
 * @param entries Buffer to read data into
 * @return Number of entries listed
 */
u32 Directory_SDMC::Read(const u32 count, Entry* entries) {
    u32 entries_read = 0;

    while (entries_read < count && children_iterator != directory.children.cend()) {
        const FileUtil::FSTEntry& file = *children_iterator;
        const std::string& filename = file.virtualName;
        Entry& entry = entries[entries_read];

        WARN_LOG(FILESYS, "File %s: size=%llu dir=%d", filename.c_str(), file.size, file.isDirectory);

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

/**
 * Close the directory
 * @return true if the directory closed correctly
 */
bool Directory_SDMC::Close() const {
    return true;
}

} // namespace FileSys
