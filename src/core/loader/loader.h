// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

enum FileType {
    FILETYPE_ERROR,

    FILETYPE_CTR_CCI,
    FILETYPE_CTR_CIA,
    FILETYPE_CTR_CXI,
    FILETYPE_CTR_ELF,
    FILETYPE_CTR_BIN,

    FILETYPE_UNKNOWN
};

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
