// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/file_util.h"

#include "core/loader.h"
#include "core/system.h"
#include "core/core.h"
#include "core/file_sys/directory_file_system.h"
#include "core/elf/elf_reader.h"

#include "core/mem_map.h"

////////////////////////////////////////////////////////////////////////////////////////////////////

/// Loads an extracted CXI from a directory
bool LoadDirectory_CXI(std::string &filename) {
    std::string full_path = filename;
    std::string path, file, extension;
    SplitPath(ReplaceAll(full_path, "\\", "/"), &path, &file, &extension);
#if EMU_PLATFORM == PLATFORM_WINDOWS
    path = ReplaceAll(path, "/", "\\");
#endif
    DirectoryFileSystem *fs = new DirectoryFileSystem(&System::g_ctr_file_system, path);
    System::g_ctr_file_system.Mount("fs:", fs);

    std::string final_name = "fs:/" + file + extension;
    File::IOFile f(filename, "rb");

    if (f.IsOpen()) {
        // TODO(ShizZy): read here to memory....
    }
    ERROR_LOG(TIME, "Unimplemented function!");
    return true;
}

/// Loads a CTR ELF file
bool Load_ELF(std::string &filename) {
    std::string full_path = filename;
    std::string path, file, extension;
    SplitPath(ReplaceAll(full_path, "\\", "/"), &path, &file, &extension);
#if EMU_PLATFORM == PLATFORM_WINDOWS
    path = ReplaceAll(path, "/", "\\");
#endif
    File::IOFile f(filename, "rb");

    if (f.IsOpen()) {
        u64 size = f.GetSize();
        u8* buffer = new u8[size];
        ElfReader* elf_reader = NULL;

        f.ReadBytes(buffer, size);

        elf_reader = new ElfReader(buffer);
        elf_reader->LoadInto(0x00100000);

        Core::g_app_core->SetPC(elf_reader->GetEntryPoint());

        delete[] buffer;
        delete elf_reader;
    } else {
        return false;
    }
    f.Close();

    return true;
}

/// Loads a Launcher DAT file
bool Load_DAT(std::string &filename) {
    std::string full_path = filename;
    std::string path, file, extension;
    SplitPath(ReplaceAll(full_path, "\\", "/"), &path, &file, &extension);
#if EMU_PLATFORM == PLATFORM_WINDOWS
    path = ReplaceAll(path, "/", "\\");
#endif
    File::IOFile f(filename, "rb");

    if (f.IsOpen()) {
        u64 size = f.GetSize();
        u8* buffer = new u8[size];

        f.ReadBytes(buffer, size);

        /**
        * (mattvail) We shouldn't really support this type of file
        * but for the sake of making it easier... we'll temporarily/hackishly
        * allow it. No sense in making a proper reader for this.
        */
        u32 entrypoint = 0x080c3ee0; // write to same entrypoint as elf
        u32 payload_offset = 0x6F4;
        
        const u8 *src = &buffer[payload_offset];
        u8 *dst = Memory::GetPointer(entrypoint);
        u32 srcSize = size - payload_offset; //just load everything...
        u32 *s = (u32*)src;
        u32 *d = (u32*)dst;
        for (int j = 0; j < (int)(srcSize + 3) / 4; j++)
        {
            *d++ = (*s++);
        }
        
        Core::g_app_core->SetPC(entrypoint);

        delete[] buffer;
    }
    else {
        return false;
    }
    f.Close();

    return true;
}

namespace Loader {

bool IsBootableDirectory() {
    ERROR_LOG(TIME, "Unimplemented function!");
    return true;
}

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
        if (IsBootableDirectory()) {
            return FILETYPE_DIRECTORY_CXI;
        }
        else {
            return FILETYPE_NORMAL_DIRECTORY;
        }
    }
    else if (!strcasecmp(extension.c_str(), ".elf")) {
        return FILETYPE_CTR_ELF; // TODO(bunnei): Do some filetype checking :p
    }
    else if (!strcasecmp(extension.c_str(), ".dat")) {
        return FILETYPE_LAUNCHER_DAT;
    }
    else if (!strcasecmp(extension.c_str(), ".zip")) {
        return FILETYPE_ARCHIVE_ZIP;
    }
    else if (!strcasecmp(extension.c_str(), ".rar")) {
        return FILETYPE_ARCHIVE_RAR;
    }
    else if (!strcasecmp(extension.c_str(), ".r00")) {
        return FILETYPE_ARCHIVE_RAR;
    }
    else if (!strcasecmp(extension.c_str(), ".r01")) {
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
    INFO_LOG(LOADER, "Identifying file...");

    // Note that this can modify filename!
    switch (IdentifyFile(filename)) {

    case FILETYPE_CTR_ELF:
        return Load_ELF(filename);

    case FILETYPE_LAUNCHER_DAT:
        return Load_DAT(filename);

    case FILETYPE_DIRECTORY_CXI:
        return LoadDirectory_CXI(filename);

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