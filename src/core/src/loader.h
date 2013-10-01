/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    loader.h
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2013-09-18
 * @brief   Loads bootable binaries into the emu
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#ifndef CORE_LOADER_H_
#define CORE_LOADER_H_

#include "common.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Loader {

enum FileType {
	FILETYPE_ERROR,

	FILETYPE_CTR_CCI,
	FILETYPE_CTR_CIA,
	FILETYPE_CTR_CXI,
	FILETYPE_CTR_ELF,

	FILETYPE_CTR_DIRECTORY,

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

#endif // CORE_LOADER_H_