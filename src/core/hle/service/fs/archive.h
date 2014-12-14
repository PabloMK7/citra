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
ResultCode DeleteFileFromArchive(Handle archive_handle, const FileSys::Path& path);

/**
 * Rename a File between two Archives
 * @param src_archive_handle Handle to the source Archive object
 * @param src_path Path to the File inside of the source Archive
 * @param dest_archive_handle Handle to the destination Archive object
 * @param dest_path Path to the File inside of the destination Archive
 * @return Whether rename succeeded
 */
ResultCode RenameFileBetweenArchives(Handle src_archive_handle, const FileSys::Path& src_path,
                                     Handle dest_archive_handle, const FileSys::Path& dest_path);

/**
 * Delete a Directory from an Archive
 * @param archive_handle Handle to an open Archive object
 * @param path Path to the Directory inside of the Archive
 * @return Whether deletion succeeded
 */
ResultCode DeleteDirectoryFromArchive(Handle archive_handle, const FileSys::Path& path);

/**
 * Create a Directory from an Archive
 * @param archive_handle Handle to an open Archive object
 * @param path Path to the Directory inside of the Archive
 * @return Whether creation of directory succeeded
 */
ResultCode CreateDirectoryFromArchive(Handle archive_handle, const FileSys::Path& path);

/**
 * Rename a Directory between two Archives
 * @param src_archive_handle Handle to the source Archive object
 * @param src_path Path to the Directory inside of the source Archive
 * @param dest_archive_handle Handle to the destination Archive object
 * @param dest_path Path to the Directory inside of the destination Archive
 * @return Whether rename succeeded
 */
ResultCode RenameDirectoryBetweenArchives(Handle src_archive_handle, const FileSys::Path& src_path,
                                          Handle dest_archive_handle, const FileSys::Path& dest_path);

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
