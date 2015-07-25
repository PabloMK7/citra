// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <string>

#include "common/logging/log.h"
#include "common/make_unique.h"
#include "common/string_util.h"

#include "core/file_sys/archive_romfs.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/fs/archive.h"
#include "core/loader/3dsx.h"
#include "core/loader/elf.h"
#include "core/loader/ncch.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

namespace Loader {

const std::initializer_list<Kernel::AddressMapping> default_address_mappings = {
    { 0x1FF50000,   0x8000, true  }, // part of DSP RAM
    { 0x1FF70000,   0x8000, true  }, // part of DSP RAM
    { 0x1F000000, 0x600000, false }, // entire VRAM
};

/**
 * Identifies the type of a bootable file
 * @param file open file
 * @return FileType of file
 */
static FileType IdentifyFile(FileUtil::IOFile& file) {
    FileType type;

#define CHECK_TYPE(loader) \
    type = AppLoader_##loader::IdentifyType(file); \
    if (FileType::Error != type) \
        return type;

    CHECK_TYPE(THREEDSX)
    CHECK_TYPE(ELF)
    CHECK_TYPE(NCCH)

#undef CHECK_TYPE

    return FileType::Unknown;
}

/**
 * Guess the type of a bootable file from its extension
 * @param extension String extension of bootable file
 * @return FileType of file
 */
static FileType GuessFromExtension(const std::string& extension_) {
    std::string extension = Common::ToLower(extension_);

    if (extension == ".elf" || extension == ".axf")
        return FileType::ELF;

    if (extension == ".cci" || extension == ".3ds")
        return FileType::CCI;

    if (extension == ".cxi")
        return FileType::CXI;

    if (extension == ".3dsx")
        return FileType::THREEDSX;

    return FileType::Unknown;
}

static const char* GetFileTypeString(FileType type) {
    switch (type) {
    case FileType::CCI:
        return "NCSD";
    case FileType::CXI:
        return "NCCH";
    case FileType::ELF:
        return "ELF";
    case FileType::THREEDSX:
        return "3DSX";
    case FileType::Error:
    case FileType::Unknown:
        break;
    }

    return "unknown";
}

ResultStatus LoadFile(const std::string& filename) {
    FileUtil::IOFile file(filename, "rb");
    if (!file.IsOpen()) {
        LOG_ERROR(Loader, "Failed to load file %s", filename.c_str());
        return ResultStatus::Error;
    }

    std::string filename_filename, filename_extension;
    Common::SplitPath(filename, nullptr, &filename_filename, &filename_extension);

    FileType type = IdentifyFile(file);
    FileType filename_type = GuessFromExtension(filename_extension);

    if (type != filename_type) {
        LOG_WARNING(Loader, "File %s has a different type than its extension.", filename.c_str());
        if (FileType::Unknown == type)
            type = filename_type;
    }

    LOG_INFO(Loader, "Loading file %s as %s...", filename.c_str(), GetFileTypeString(type));

    switch (type) {

    //3DSX file format...
    case FileType::THREEDSX:
        return AppLoader_THREEDSX(std::move(file), filename_filename).Load();

    // Standard ELF file format...
    case FileType::ELF:
        return AppLoader_ELF(std::move(file), filename_filename).Load();

    // NCCH/NCSD container formats...
    case FileType::CXI:
    case FileType::CCI:
    {
        AppLoader_NCCH app_loader(std::move(file), filename);

        // Load application and RomFS
        if (ResultStatus::Success == app_loader.Load()) {
            Service::FS::RegisterArchiveType(Common::make_unique<FileSys::ArchiveFactory_RomFS>(app_loader), Service::FS::ArchiveIdCode::RomFS);
            return ResultStatus::Success;
        }
        break;
    }

    // Error occurred durring IdentifyFile...
    case FileType::Error:

    // IdentifyFile could know identify file type...
    case FileType::Unknown:
    {
        LOG_CRITICAL(Loader, "File %s is of unknown type.", filename.c_str());
        return ResultStatus::ErrorInvalidFormat;
    }
    }
    return ResultStatus::Error;
}

} // namespace Loader
