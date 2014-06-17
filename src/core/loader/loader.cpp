// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "core/loader/loader.h"
#include "core/loader/elf.h"
#include "core/loader/ncch.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

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

    if (!strcasecmp(extension.c_str(), ".elf")) {
        return FILETYPE_CTR_ELF; // TODO(bunnei): Do some filetype checking :p
    }
    else if (!strcasecmp(extension.c_str(), ".axf")) {
        return FILETYPE_CTR_ELF; // TODO(bunnei): Do some filetype checking :p
    }
    else if (!strcasecmp(extension.c_str(), ".cxi")) {
        return FILETYPE_CTR_CXI; // TODO(bunnei): Do some filetype checking :p
    }
    else if (!strcasecmp(extension.c_str(), ".cci")) {
        return FILETYPE_CTR_CCI; // TODO(bunnei): Do some filetype checking :p
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
    INFO_LOG(LOADER, "Identifying file...");

    // Note that this can modify filename!
    switch (IdentifyFile(filename)) {

    case FILETYPE_CTR_ELF:
        return Loader::Load_ELF(filename, error_string);

    case FILETYPE_CTR_CXI:
    case FILETYPE_CTR_CCI:
        return Loader::Load_NCCH(filename, error_string);

    case FILETYPE_ERROR:
        ERROR_LOG(LOADER, "Could not read file");
        *error_string = "Error reading file";
        break;

    case FILETYPE_UNKNOWN:
    default:
        ERROR_LOG(LOADER, "Failed to identify file");
        *error_string = " Failed to identify file";
        break;
    }
    return false;
}

} // namespace Loader
