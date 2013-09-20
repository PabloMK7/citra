/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    loader.cpp
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

#include "file_util.h"
#include "loader.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

bool _Load_BIN(std::string &filename) {
	File::IOFile f(filename, "rb");
	if (f.IsOpen()) {
		// TODO(ShizZy): read here to memory....
	}
	return true;
}

namespace Loader {

/**
 * Identifies the type of a bootable file
 * @param filename String filename of bootable file
 * @todo (ShizZy) this function sucks... make it actually check file contents etc.
 * @return FileType of file
 */
FileType IdentifyFile(std::string &filename) {
	if (filename.size() == 0) {
		ERROR_LOG(LOADER, "invalid filename %s", filename.c_str());
		return FILETYPE_ERROR;
	}
	std::string extension = filename.size() >= 5 ? filename.substr(filename.size() - 4) : "";
	
	if (File::IsDirectory(filename)) {
		return FILETYPE_NORMAL_DIRECTORY;
	} else if (!strcasecmp(extension.c_str(),".bin")) {
		return FILETYPE_3DS_BIN;
	} else if (!strcasecmp(extension.c_str(),".zip")) {
		return FILETYPE_ARCHIVE_ZIP;
	} else if (!strcasecmp(extension.c_str(),".rar")) {
		return FILETYPE_ARCHIVE_RAR;
	} else if (!strcasecmp(extension.c_str(),".r00")) {
		return FILETYPE_ARCHIVE_RAR;
	} else if (!strcasecmp(extension.c_str(),".r01")) {
		return FILETYPE_ARCHIVE_RAR;
	}
	return FILETYPE_UNKNOWN;
}

/**
 * Identifies and loads a bootable file
 * @param filename String filename of bootable file
 * @param error_string Point to string to put error message if an error has occurred
 * @return True on success, otherwise false
 */
bool LoadFile(std::string &filename, std::string *error_string) {
	INFO_LOG(LOADER,"Identifying file...");
	// Note that this can modify filename!
	switch (IdentifyFile(filename)) {
	
	case FILETYPE_3DS_BIN:
		{
			INFO_LOG(LOADER,"File is a BIN !");
			return _Load_BIN(filename);
		}

	case FILETYPE_ERROR:
		ERROR_LOG(LOADER, "Could not read file");
		*error_string = "Error reading file";
		break;

	case FILETYPE_ARCHIVE_RAR:
#ifdef WIN32
		*error_string = "RAR file detected (Require WINRAR)";
#else
		*error_string = "RAR file detected (Require UnRAR)";
#endif
		break;

	case FILETYPE_ARCHIVE_ZIP:
#ifdef WIN32
		*error_string = "ZIP file detected (Require WINRAR)";
#else
		*error_string = "ZIP file detected (Require UnRAR)";
#endif
		break;

	case FILETYPE_NORMAL_DIRECTORY:
		ERROR_LOG(LOADER, "Just a directory.");
		*error_string = "Just a directory.";
		break;

	case FILETYPE_UNKNOWN_BIN:
	case FILETYPE_UNKNOWN_ELF:
	case FILETYPE_UNKNOWN:
	default:
		ERROR_LOG(LOADER, "Failed to identify file");
		*error_string = "Failed to identify file";
		break;
	}
	return false;
}

} // namespace