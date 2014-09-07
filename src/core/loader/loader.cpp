// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <memory>

#include "core/file_sys/archive_romfs.h"
#include "core/loader/loader.h"
#include "core/loader/elf.h"
#include "core/loader/ncch.h"
#include "core/hle/kernel/archive.h"
#include "core/mem_map.h"

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

    size_t extension_loc = filename.find_last_of('.');
    std::string extension = extension_loc != std::string::npos ? filename.substr(extension_loc) : "";

    if (LowerStr(extension) == ".elf") {
        return FileType::ELF; // TODO(bunnei): Do some filetype checking :p
    }
    else if (LowerStr(extension) == ".axf") {
        return FileType::ELF; // TODO(bunnei): Do some filetype checking :p
    }
    else if (LowerStr(extension) == ".cxi") {
        return FileType::CXI; // TODO(bunnei): Do some filetype checking :p
    }
    else if (LowerStr(extension) == ".cci") {
        return FileType::CCI; // TODO(bunnei): Do some filetype checking :p
    }
    else if (LowerStr(extension) == ".bin") {
        return FileType::BIN; // TODO(bunnei): Do some filetype checking :p
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
    case FileType::ELF:
        return AppLoader_ELF(filename).Load();

    // NCCH/NCSD container formats...
    case FileType::CXI:
    case FileType::CCI: {
        AppLoader_NCCH app_loader(filename);

        // Load application and RomFS
        if (ResultStatus::Success == app_loader.Load()) {
            Kernel::CreateArchive(new FileSys::Archive_RomFS(app_loader), "RomFS");
            return ResultStatus::Success;
        }
        break;
    }

    // Raw BIN file format...
    case FileType::BIN:
    {
        INFO_LOG(LOADER, "Loading BIN file %s...", filename.c_str());

        File::IOFile file(filename, "rb");

        if (file.IsOpen()) {
            file.ReadBytes(Memory::GetPointer(Memory::EXEFS_CODE_VADDR), (size_t)file.GetSize());
            Kernel::LoadExec(Memory::EXEFS_CODE_VADDR);
        } else {
            return ResultStatus::Error;
        }
        return ResultStatus::Success;
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
