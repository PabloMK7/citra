// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Loader {

enum FileType {
	FILETYPE_ERROR,

	FILETYPE_CTR_CCI,
	FILETYPE_CTR_CIA,
	FILETYPE_CTR_CXI,
	FILETYPE_CTR_ELF,

	FILETYPE_DIRECTORY_CXI,

	FILETYPE_UNKNOWN_BIN,
	FILETYPE_UNKNOWN_ELF,

	FILETYPE_ARCHIVE_RAR,
	FILETYPE_ARCHIVE_ZIP,

	FILETYPE_NORMAL_DIRECTORY,

	FILETYPE_UNKNOWN
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Identifies the type of a bootable file
 * @param filename String filename of bootable file
 * @return FileType of file
 */
FileType IdentifyFile(std::string &filename);

/**
 * Identifies and loads a bootable file
 * @param filename String filename of bootable file
 * @param error_string Point to string to put error message if an error has occurred
 * @return True on success, otherwise false
 */
bool LoadFile(std::string &filename, std::string *error_string);

} // namespace
