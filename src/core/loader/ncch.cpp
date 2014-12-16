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
static u32 LZSS_GetDecompressedSize(u8* buffer, u32 size) {
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
static bool LZSS_Decompress(u8* compressed, u32 compressed_size, u8* decompressed, u32 decompressed_size) {
    u8* footer = compressed + compressed_size - 8;
    u32 buffer_top_and_bottom = *(u32*)footer;
    u32 out = decompressed_size;
    u32 index = compressed_size - ((buffer_top_and_bottom >> 24) & 0xFF);
    u32 stop_index = compressed_size - (buffer_top_and_bottom & 0xFFFFFF);

    memset(decompressed, 0, decompressed_size);
    memcpy(decompressed, compressed, compressed_size);

    while(index > stop_index) {
       u8 control = compressed[--index];

        for(u32 i = 0; i < 8; i++) {
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
                for(u32 j = 0; j < segment_size; j++) {
                    // Check if compression is out of bounds
                    if(out + segment_offset >= decompressed_size) {
                        return false;
                    }

                    u8 data  = decompressed[out + segment_offset];
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
}

/**
 * Loads .code section into memory for booting
 * @return ResultStatus result of function
 */
ResultStatus AppLoader_NCCH::LoadExec() const {
    if (!is_loaded)
        return ResultStatus::ErrorNotLoaded;

    std::vector<u8> code;
    if (ResultStatus::Success == ReadCode(code)) {
        Memory::WriteBlock(entry_point, &code[0], code.size());
        Kernel::LoadExec(entry_point);
        return ResultStatus::Success;
    }
    return ResultStatus::Error;
}

/**
 * Reads an application ExeFS section of an NCCH file into AppLoader (e.g. .code, .logo, etc.)
 * @param name Name of section to read out of NCCH file
 * @param buffer Vector to read data into
 * @return ResultStatus result of function
 */
ResultStatus AppLoader_NCCH::LoadSectionExeFS(const char* name, std::vector<u8>& buffer) const {
    // Iterate through the ExeFs archive until we find the .code file...
    FileUtil::IOFile file(filename, "rb");
    if (file.IsOpen()) {
        LOG_DEBUG(Loader, "%d sections:", kMaxSections);
        for (int i = 0; i < kMaxSections; i++) {
            // Load the specified section...
            if (strcmp((const char*)exefs_header.section[i].name, name) == 0) {
                LOG_DEBUG(Loader, "%d - offset: 0x%08X, size: 0x%08X, name: %s", i,
                        exefs_header.section[i].offset, exefs_header.section[i].size,
                        exefs_header.section[i].name);

                s64 section_offset = (exefs_header.section[i].offset + exefs_offset +
                    sizeof(ExeFs_Header)+ncch_offset);
                file.Seek(section_offset, 0);

                // Section is compressed...
                if (i == 0 && is_compressed) {
                    // Read compressed .code section...
                    std::unique_ptr<u8[]> temp_buffer;
                    try {
                        temp_buffer.reset(new u8[exefs_header.section[i].size]);
                    } catch (std::bad_alloc&) {
                        return ResultStatus::ErrorMemoryAllocationFailed;
                    }
                    file.ReadBytes(&temp_buffer[0], exefs_header.section[i].size);

                    // Decompress .code section...
                    u32 decompressed_size = LZSS_GetDecompressedSize(&temp_buffer[0],
                        exefs_header.section[i].size);
                    buffer.resize(decompressed_size);
                    if (!LZSS_Decompress(&temp_buffer[0], exefs_header.section[i].size, &buffer[0],
                        decompressed_size)) {
                        return ResultStatus::ErrorInvalidFormat;
                    }
                    // Section is uncompressed...
                }
                else {
                    buffer.resize(exefs_header.section[i].size);
                    file.ReadBytes(&buffer[0], exefs_header.section[i].size);
                }
                return ResultStatus::Success;
            }
        }
    } else {
        LOG_ERROR(Loader, "Unable to read file %s!", filename.c_str());
        return ResultStatus::Error;
    }
    return ResultStatus::ErrorNotUsed;
}

/**
 * Loads an NCCH file (e.g. from a CCI, or the first NCCH in a CXI)
 * @param error_string Pointer to string to put error message if an error has occurred
 * @todo Move NCSD parsing out of here and create a separate function for loading these
 * @return True on success, otherwise false
 */
ResultStatus AppLoader_NCCH::Load() {
    LOG_INFO(Loader, "Loading NCCH file %s...", filename.c_str());

    if (is_loaded)
        return ResultStatus::ErrorAlreadyLoaded;

    FileUtil::IOFile file(filename, "rb");
    if (file.IsOpen()) {
        file.ReadBytes(&ncch_header, sizeof(NCCH_Header));

        // Skip NCSD header and load first NCCH (NCSD is just a container of NCCH files)...
        if (0 == memcmp(&ncch_header.magic, "NCSD", 4)) {
            LOG_WARNING(Loader, "Only loading the first (bootable) NCCH within the NCSD file!");
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

        LOG_INFO(Loader, "Name:            %s", exheader_header.codeset_info.name);
        LOG_DEBUG(Loader, "Code compressed: %s", is_compressed ? "yes" : "no");
        LOG_DEBUG(Loader, "Entry point:     0x%08X", entry_point);

        // Read ExeFS...

        exefs_offset = ncch_header.exefs_offset * kBlockSize;
        u32 exefs_size = ncch_header.exefs_size * kBlockSize;

        LOG_DEBUG(Loader, "ExeFS offset:    0x%08X", exefs_offset);
        LOG_DEBUG(Loader, "ExeFS size:      0x%08X", exefs_size);

        file.Seek(exefs_offset + ncch_offset, 0);
        file.ReadBytes(&exefs_header, sizeof(ExeFs_Header));

        is_loaded = true; // Set state to loaded

        LoadExec(); // Load the executable into memory for booting

        return ResultStatus::Success;
    } else {
        LOG_ERROR(Loader, "Unable to read file %s!", filename.c_str());
    }
    return ResultStatus::Error;
}

/**
 * Get the code (typically .code section) of the application
 * @param buffer Reference to buffer to store data
 * @return ResultStatus result of function
 */
ResultStatus AppLoader_NCCH::ReadCode(std::vector<u8>& buffer) const {
    return LoadSectionExeFS(".code", buffer);
}

/**
 * Get the icon (typically icon section) of the application
 * @param buffer Reference to buffer to store data
 * @return ResultStatus result of function
 */
ResultStatus AppLoader_NCCH::ReadIcon(std::vector<u8>& buffer) const {
    return LoadSectionExeFS("icon", buffer);
}

/**
 * Get the banner (typically banner section) of the application
 * @param buffer Reference to buffer to store data
 * @return ResultStatus result of function
 */
ResultStatus AppLoader_NCCH::ReadBanner(std::vector<u8>& buffer) const {
    return LoadSectionExeFS("banner", buffer);
}

/**
 * Get the logo (typically logo section) of the application
 * @param buffer Reference to buffer to store data
 * @return ResultStatus result of function
 */
ResultStatus AppLoader_NCCH::ReadLogo(std::vector<u8>& buffer) const {
    return LoadSectionExeFS("logo", buffer);
}

/**
 * Get the RomFS of the application
 * @param buffer Reference to buffer to store data
 * @return ResultStatus result of function
 */
ResultStatus AppLoader_NCCH::ReadRomFS(std::vector<u8>& buffer) const {
    FileUtil::IOFile file(filename, "rb");
    if (file.IsOpen()) {
        // Check if the NCCH has a RomFS...
        if (ncch_header.romfs_offset != 0 && ncch_header.romfs_size != 0) {
            u32 romfs_offset = ncch_offset + (ncch_header.romfs_offset * kBlockSize) + 0x1000;
            u32 romfs_size = (ncch_header.romfs_size * kBlockSize) - 0x1000;

            LOG_DEBUG(Loader, "RomFS offset:    0x%08X", romfs_offset);
            LOG_DEBUG(Loader, "RomFS size:      0x%08X", romfs_size);

            buffer.resize(romfs_size);

            file.Seek(romfs_offset, 0);
            file.ReadBytes(&buffer[0], romfs_size);

            return ResultStatus::Success;
        }
        LOG_DEBUG(Loader, "NCCH has no RomFS");
        return ResultStatus::ErrorNotUsed;
    } else {
        LOG_ERROR(Loader, "Unable to read file %s!", filename.c_str());
    }
    return ResultStatus::Error;
}

u64 AppLoader_NCCH::GetProgramId() const {
    return *reinterpret_cast<u64 const*>(&ncch_header.program_id[0]);
}

} // namespace Loader
