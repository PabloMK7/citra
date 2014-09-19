// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/hle/kernel/kernel.h"
#include "core/file_sys/archive.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Kernel namespace

namespace Kernel {

/**
 * Opens an archive
 * @param id_code IdCode of the archive to open
 * @return Handle to archive if it exists, otherwise a null handle (0)
 */
Handle OpenArchive(FileSys::Archive::IdCode id_code);

/**
 * Closes an archive
 * @param id_code IdCode of the archive to open
 * @return true if it worked fine
 */
Result CloseArchive(FileSys::Archive::IdCode id_code);

/**
 * Creates an Archive
 * @param backend File system backend interface to the archive
 * @param name Optional name of Archive
 * @return Handle to newly created Archive object
 */
Handle CreateArchive(FileSys::Archive* backend, const std::string& name);

/**
 * Open a File from an Archive
 * @param archive_handle Handle to an open Archive object
 * @param path Path to the File inside of the Archive
 * @param mode Mode under which to open the File
 * @return Opened File object
 */
Handle OpenFileFromArchive(Handle handle, const std::string& name, const FileSys::Mode mode);

/**
 * Open a Directory from an Archive
 * @param archive_handle Handle to an open Archive object
 * @param path Path to the Directory inside of the Archive
 * @return Opened Directory object
 */
Handle OpenDirectoryFromArchive(Handle handle, const std::string& name);

/// Initialize archives
void ArchiveInit();

/// Shutdown archives
void ArchiveShutdown();

} // namespace FileSys
