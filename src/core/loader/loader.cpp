// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <memory>

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
FileType IdentifyFile(const std::string &filename) {
    if (filename.size() == 0) {
        ERROR_LOG(LOADER, "invalid filename %s", filename.c_str());
        return FileType::Error;
    }
    std::string extension = filename.size() >= 5 ? filename.substr(filename.size() - 4) : "";

    if (!strcasecmp(extension.c_str(), ".elf")) {
        return FileType::ELF; // TODO(bunnei): Do some filetype checking :p
    }
    else if (!strcasecmp(extension.c_str(), ".axf")) {
        return FileType::ELF; // TODO(bunnei): Do some filetype checking :p
    }
    else if (!strcasecmp(extension.c_str(), ".cxi")) {
        return FileType::CXI; // TODO(bunnei): Do some filetype checking :p
    }
    else if (!strcasecmp(extension.c_str(), ".cci")) {
        return FileType::CCI; // TODO(bunnei): Do some filetype checking :p
    }
    return FileType::Unknown;
}

/**
 * Identifies and loads a bootable file
 * @param filename String filename of bootable file
 * @return ResultStatus result of function
 */
ResultStatus LoadFile(const std::string& filename) {
    INFO_LOG(LOADER, "Loading file %s...", filename.c_str());

    switch (IdentifyFile(filename)) {

    // Standard ELF file format...
    case FileType::ELF: {
        return AppLoader_ELF(filename).Load();
    }

    // NCCH/NCSD container formats...
    case FileType::CXI:
    case FileType::CCI: {
        return AppLoader_NCCH(filename).Load();
    }

    // Error occurred durring IdentifyFile...
    case FileType::Error:

    // IdentifyFile could know identify file type...
    case FileType::Unknown:

    default:
        return ResultStatus::ErrorInvalidFormat;
    }

    return ResultStatus::Error;
}

} // namespace Loader
