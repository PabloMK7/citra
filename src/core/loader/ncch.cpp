// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <memory>

#include "common/file_util.h"

#include "core/loader/ncch.h"
#include "core/hle/kernel/kernel.h"
#include "core/mem_map.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

static const int kMaxSections   = 8;        ///< Maximum number of sections (files) in an ExeFs
static const int kBlockSize     = 0x200;    ///< Size of ExeFS blocks (in bytes)

/**
 * Get the decompressed size of an LZSS compressed ExeFS file
 * @param buffer Buffer of compressed file
 * @param size Size of compressed buffer
 * @return Size of decompressed buffer
 */
u32 LZSS_GetDecompressedSize(u8* buffer, u32 size) {
    u32 offset_size = *(u32*)(buffer + size - 4);
    return offset_size + size;
}

/**
 * Decompress ExeFS file (compressed with LZSS)
 * @param compressed Compressed buffer
 * @param compressed_size Size of compressed buffer
 * @param decompressed Decompressed buffer
 * @param decompressed_size Size of decompressed buffer
 * @return True on success, otherwise false
 */
bool LZSS_Decompress(u8* compressed, u32 compressed_size, u8* decompressed, u32 decompressed_size) {
    u8* footer = compressed + compressed_size - 8;
    u32 buffer_top_and_bottom = *(u32*)footer;
    u32 i, j;
    u32 out = decompressed_size;
    u32 index = compressed_size - ((buffer_top_and_bottom >> 24) & 0xFF);
    u8 control;
    u32 stop_index = compressed_size - (buffer_top_and_bottom & 0xFFFFFF);

    memset(decompressed, 0, decompressed_size);
    memcpy(decompressed, compressed, compressed_size);

    while(index > stop_index) {
        control = compressed[--index];

        for(i = 0; i < 8; i++) {
            if(index <= stop_index)
                break;
            if(index <= 0)
                break;
            if(out <= 0)
                break;

            if(control & 0x80) {
                // Check if compression is out of bounds
                if(index < 2) {
                    return false;
                }
                index -= 2;

                u32 segment_offset = compressed[index] | (compressed[index + 1] << 8);
                u32 segment_size = ((segment_offset >> 12) & 15) + 3;
                segment_offset &= 0x0FFF;
                segment_offset += 2;

                // Check if compression is out of bounds
                if(out < segment_size) {
                    return false;
                }
                for(j = 0; j < segment_size; j++) {
                    u8 data;
                    // Check if compression is out of bounds
                    if(out + segment_offset >= decompressed_size) {
                        return false;
                    }
                    data  = decompressed[out + segment_offset];
                    decompressed[--out] = data;
                }
            } else {
                // Check if compression is out of bounds
                if(out < 1) {
                    return false;
                }
                decompressed[--out] = compressed[--index];
            }
            control <<= 1;
        }
    }
    return true;
}

////////////////////////////////////////////////////////////////////////////////////////////////////
// AppLoader_NCCH class

/// AppLoader_NCCH constructor
AppLoader_NCCH::AppLoader_NCCH(const std::string& filename) {
    this->filename = filename;
    is_loaded = false;
    is_compressed = false;
    entry_point = 0;
    ncch_offset = 0;
    exefs_offset = 0;
}

/// AppLoader_NCCH destructor
AppLoader_NCCH::~AppLoader_NCCH() {
    if (file.IsOpen())
        file.Close();
}

/**
 * Loads .code section into memory for booting
 * @return ResultStatus result of function
 */
ResultStatus AppLoader_NCCH::LoadExec() {
    if (!is_loaded) 
        return ResultStatus::ErrorNotLoaded;

    ResultStatus res;
    code = ReadCode(res);

    if (ResultStatus::Success == res) {
        Memory::WriteBlock(entry_point, &code[0], code.size());
        Kernel::LoadExec(entry_point);
    }
    return res;
}

/**
 * Reads an application ExeFS section of an NCCH file into AppLoader (e.g. .code, .logo, etc.)
 * @param name Name of section to read out of NCCH file
 * @param buffer Vector to read data into
 * @param error ResultStatus result of function
 * @return Reference to buffer of data that was read
 */
const std::vector<u8>& AppLoader_NCCH::LoadSectionExeFS(const char* name, std::vector<u8>& buffer, 
    ResultStatus& error) {
    // Iterate through the ExeFs archive until we find the .code file...
    for (int i = 0; i < kMaxSections; i++) {
        // Load the specified section...
        if (strcmp((const char*)exefs_header.section[i].name, name) == 0) {
            INFO_LOG(LOADER, "ExeFS section %d:", i);
            INFO_LOG(LOADER, "    name:   %s", exefs_header.section[i].name);
            INFO_LOG(LOADER, "    offset: 0x%08X", exefs_header.section[i].offset);
            INFO_LOG(LOADER, "    size:   0x%08X", exefs_header.section[i].size);

            s64 section_offset = (exefs_header.section[i].offset + exefs_offset + 
                sizeof(ExeFs_Header) + ncch_offset);
            file.Seek(section_offset, 0);

            // Section is compressed...
            if (i == 0 && is_compressed) {
                // Read compressed .code section...
                std::unique_ptr<u8[]> temp_buffer(new u8[exefs_header.section[i].size]);
                file.ReadBytes(&temp_buffer[0], exefs_header.section[i].size);

                // Decompress .code section...
                u32 decompressed_size = LZSS_GetDecompressedSize(&temp_buffer[0], 
                    exefs_header.section[i].size);
                buffer.resize(decompressed_size);
                if (!LZSS_Decompress(&temp_buffer[0], exefs_header.section[i].size, &buffer[0],
                    decompressed_size)) {
                    error = ResultStatus::ErrorInvalidFormat;
                    return buffer;
                }
            // Section is uncompressed...
            } else {
                buffer.resize(exefs_header.section[i].size);
                file.ReadBytes(&buffer[0], exefs_header.section[i].size);
            }
            error = ResultStatus::Success;
            return buffer;
        }
    }
    error = ResultStatus::ErrorNotUsed;
    return buffer;
} 

/**
 * Loads an NCCH file (e.g. from a CCI, or the first NCCH in a CXI)
 * @param error_string Pointer to string to put error message if an error has occurred
 * @todo Move NCSD parsing out of here and create a separate function for loading these
 * @return True on success, otherwise false
 */
ResultStatus AppLoader_NCCH::Load() {
    INFO_LOG(LOADER, "Loading NCCH file %s...", filename.c_str());

    if (is_loaded)
        return ResultStatus::ErrorAlreadyLoaded;

    file = File::IOFile(filename, "rb");

    if (file.IsOpen()) {
        file.ReadBytes(&ncch_header, sizeof(NCCH_Header));

        // Skip NCSD header and load first NCCH (NCSD is just a container of NCCH files)...
        if (0 == memcmp(&ncch_header.magic, "NCSD", 4)) {
            WARN_LOG(LOADER, "Only loading the first (bootable) NCCH within the NCSD file!");
            ncch_offset = 0x4000;
            file.Seek(ncch_offset, 0);
            file.ReadBytes(&ncch_header, sizeof(NCCH_Header));
        }
        
        // Verify we are loading the correct file type...
        if (0 != memcmp(&ncch_header.magic, "NCCH", 4))
            return ResultStatus::ErrorInvalidFormat;

        // Read ExHeader...

        file.ReadBytes(&exheader_header, sizeof(ExHeader_Header));

        is_compressed = (exheader_header.codeset_info.flags.flag & 1) == 1;
        entry_point = exheader_header.codeset_info.text.address;

        INFO_LOG(LOADER, "Name:            %s", exheader_header.codeset_info.name);
        INFO_LOG(LOADER, "Code compressed: %s", is_compressed ? "yes" : "no");
        INFO_LOG(LOADER, "Entry point:     0x%08X", entry_point);

        // Read ExeFS...

        exefs_offset = ncch_header.exefs_offset * kBlockSize;
        u32 exefs_size = ncch_header.exefs_size * kBlockSize;

        INFO_LOG(LOADER, "ExeFS offset:    0x%08X", exefs_offset);
        INFO_LOG(LOADER, "ExeFS size:      0x%08X", exefs_size);

        file.Seek(exefs_offset + ncch_offset, 0);
        file.ReadBytes(&exefs_header, sizeof(ExeFs_Header));

        is_loaded = true; // Set state to loaded

        LoadExec(); // Load the executable into memory for booting

        return ResultStatus::Success;
    }
    return ResultStatus::Error;
}

/**
 * Get the code (typically .code section) of the application
 * @param error ResultStatus result of function
 * @return Reference to code buffer
 */
const std::vector<u8>& AppLoader_NCCH::ReadCode(ResultStatus& error) {
    return LoadSectionExeFS(".code", code, error);
}

/**
 * Get the icon (typically icon section) of the application
 * @param error ResultStatus result of function
 * @return Reference to icon buffer
 */
const std::vector<u8>& AppLoader_NCCH::ReadIcon(ResultStatus& error) {
    return LoadSectionExeFS("icon", icon, error);
}

/**
 * Get the banner (typically banner section) of the application
 * @param error ResultStatus result of function
 * @return Reference to banner buffer
 */
const std::vector<u8>& AppLoader_NCCH::ReadBanner(ResultStatus& error) {
    return LoadSectionExeFS("banner", banner, error);
}

/**
 * Get the logo (typically logo section) of the application
 * @param error ResultStatus result of function
 * @return Reference to logo buffer
 */
const std::vector<u8>& AppLoader_NCCH::ReadLogo(ResultStatus& error) {
    return LoadSectionExeFS("logo", logo, error);
}

/**
 * Get the RomFs archive of the application
 * @param error ResultStatus result of function
 * @return Reference to RomFs archive buffer
 */
const std::vector<u8>& AppLoader_NCCH::ReadRomFS(ResultStatus& error) {
    // Check if the NCCH has a RomFS...
    if (ncch_header.romfs_offset != 0 && ncch_header.romfs_size != 0) {
        u32 romfs_offset = ncch_offset + (ncch_header.romfs_offset * kBlockSize) + 0x1000;
        u32 romfs_size = (ncch_header.romfs_size * kBlockSize) - 0x1000;

        INFO_LOG(LOADER, "RomFS offset:    0x%08X", romfs_offset);
        INFO_LOG(LOADER, "RomFS size:      0x%08X", romfs_size);

        romfs.resize(romfs_size);

        file.Seek(romfs_offset, 0);
        file.ReadBytes(&romfs[0], romfs_size);

        error = ResultStatus::Success;
        return romfs;
    } else {
        NOTICE_LOG(LOADER, "RomFS unused");
    }
    error = ResultStatus::ErrorNotUsed;
    return romfs;
}

} // namespace Loader
