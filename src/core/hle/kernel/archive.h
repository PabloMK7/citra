// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

#include "core/file_sys/archive.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/result.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Kernel namespace

namespace Kernel {

/**
 * Opens an archive
 * @param id_code IdCode of the archive to open
 * @return Handle to the opened archive
 */
ResultVal<Handle> OpenArchive(FileSys::Archive::IdCode id_code);

/**
 * Closes an archive
 * @param id_code IdCode of the archive to open
 */
ResultCode CloseArchive(FileSys::Archive::IdCode id_code);

/**
 * Creates an Archive
 * @param backend File system backend interface to the archive
 * @param name Name of Archive
 */
ResultCode CreateArchive(FileSys::Archive* backend, const std::string& name);

/**
 * Open a File from an Archive
 * @param archive_handle Handle to an open Archive object
 * @param path Path to the File inside of the Archive
 * @param mode Mode under which to open the File
 * @return Handle to the opened File object
 */
ResultVal<Handle> OpenFileFromArchive(Handle archive_handle, const FileSys::Path& path, const FileSys::Mode mode);

/**
 * Delete a File from an Archive
 * @param archive_handle Handle to an open Archive object
 * @param path Path to the File inside of the Archive
 * @return Whether deletion succeeded
 */
Result DeleteFileFromArchive(Handle archive_handle, const FileSys::Path& path);

/**
 * Delete a Directory from an Archive
 * @param archive_handle Handle to an open Archive object
 * @param path Path to the Directory inside of the Archive
 * @return Whether deletion succeeded
 */
Result DeleteDirectoryFromArchive(Handle archive_handle, const FileSys::Path& path);

/**
 * Create a Directory from an Archive
 * @param archive_handle Handle to an open Archive object
 * @param path Path to the Directory inside of the Archive
 * @return Whether creation of directory succeeded
 */
Result CreateDirectoryFromArchive(Handle archive_handle, const FileSys::Path& path);

/**
 * Open a Directory from an Archive
 * @param archive_handle Handle to an open Archive object
 * @param path Path to the Directory inside of the Archive
 * @return Handle to the opened File object
 */
ResultVal<Handle> OpenDirectoryFromArchive(Handle archive_handle, const FileSys::Path& path);

/// Initialize archives
void ArchiveInit();

/// Shutdown archives
void ArchiveShutdown();

} // namespace FileSys
