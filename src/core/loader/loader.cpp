// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <memory>

#include "core/file_sys/archive_romfs.h"
#include "core/loader/3dsx.h"
#include "core/loader/elf.h"
#include "core/loader/ncch.h"
#include "core/hle/service/fs/archive.h"
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
        LOG_ERROR(Loader, "invalid filename %s", filename.c_str());
        return FileType::Error;
    }

    size_t extension_loc = filename.find_last_of('.');
    if (extension_loc == std::string::npos)
        return FileType::Unknown;
    std::string extension = Common::ToLower(filename.substr(extension_loc));

    // TODO(bunnei): Do actual filetype checking instead of naively checking the extension
    if (extension == ".elf") {
        return FileType::ELF;
    } else if (extension == ".axf") {
        return FileType::ELF;
    } else if (extension == ".cxi") {
        return FileType::CXI;
    } else if (extension == ".cci") {
        return FileType::CCI;
    } else if (extension == ".bin") {
        return FileType::BIN;
    } else if (extension == ".3dsx") {
        return FileType::THREEDSX;
    }
    return FileType::Unknown;
}

/**
 * Identifies and loads a bootable file
 * @param filename String filename of bootable file
 * @return ResultStatus result of function
 */
ResultStatus LoadFile(const std::string& filename) {
    LOG_INFO(Loader, "Loading file %s...", filename.c_str());

    switch (IdentifyFile(filename)) {

    //3DSX file format...
    case FileType::THREEDSX:
        return AppLoader_THREEDSX(filename).Load();

    // Standard ELF file format...
    case FileType::ELF:
        return AppLoader_ELF(filename).Load();

    // NCCH/NCSD container formats...
    case FileType::CXI:
    case FileType::CCI: {
        AppLoader_NCCH app_loader(filename);

        // Load application and RomFS
        if (ResultStatus::Success == app_loader.Load()) {
            Kernel::g_program_id = app_loader.GetProgramId();
            Service::FS::CreateArchive(std::make_unique<FileSys::Archive_RomFS>(app_loader), Service::FS::ArchiveIdCode::RomFS);
            return ResultStatus::Success;
        }
        break;
    }

    // Raw BIN file format...
    case FileType::BIN:
    {
        LOG_INFO(Loader, "Loading BIN file %s...", filename.c_str());

        FileUtil::IOFile file(filename, "rb");

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
