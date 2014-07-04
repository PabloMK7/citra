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
 * Creates an Archive
 * @param backend File system backend interface to the archive
 * @param name Optional name of Archive
 * @return Handle to newly created Archive object
 */
Handle CreateArchive(FileSys::Archive* backend, const std::string& name);

/// Initialize archives
void ArchiveInit();

/// Shutdown archives
void ArchiveShutdown();

} // namespace FileSys
