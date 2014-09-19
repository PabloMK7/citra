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

Directory_SDMC::Directory_SDMC(const Archive_SDMC* archive, const std::string& path) {
    // TODO(Link Mauve): normalize path into an absolute path without "..", it can currently bypass
    // the root directory we set while opening the archive.
    // For example, opening /../../usr/bin can give the emulated program your installed programs.
    std::string absolute_path = archive->GetMountPoint() + path;
    entry_count = FileUtil::ScanDirectoryTree(absolute_path, entry);
    current_entry = 0;
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
    u32 i;
    for (i = 0; i < count && current_entry < entry_count; ++i) {
        FileUtil::FSTEntry file = entry.children[current_entry];
        std::string filename = file.virtualName;
        WARN_LOG(FILESYS, "File %s: size=%d dir=%d", filename.c_str(), file.size, file.isDirectory);

        Entry* entry = &entries[i];

        // TODO(Link Mauve): use a proper conversion to UTF-16.
        for (int j = 0; j < FILENAME_LENGTH; ++j) {
            entry->filename[j] = filename[j];
            if (!filename[j])
                break;
        }

        // Split the filename into 8.3 format.
        // TODO(Link Mauve): move that to common, I guess, and make it more robust to long filenames.
        std::string::size_type n = filename.rfind('.');
        if (n == std::string::npos) {
            strncpy(entry->short_name, filename.c_str(), 8);
            memset(entry->extension, '\0', 3);
        } else {
            strncpy(entry->short_name, filename.substr(0, n).c_str(), 8);
            strncpy(entry->extension, filename.substr(n + 1).c_str(), 8);
        }

        entry->is_directory = file.isDirectory;
        entry->file_size = file.size;

        // We emulate a SD card where the archive bit has never been cleared, as it would be on
        // most user SD cards.
        // Some homebrews (blargSNES for instance) are known to mistakenly use the archive bit as a
        // file bit.
        entry->is_archive = !file.isDirectory;

        ++current_entry;
    }
    return i;
}

/**
 * Close the directory
 * @return true if the directory closed correctly
 */
bool Directory_SDMC::Close() const {
    return true;
}

} // namespace FileSys
