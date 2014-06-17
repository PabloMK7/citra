// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/file_util.h"

#include "core/loader/ncch.h"
#include "core/hle/kernel/kernel.h"
#include "core/mem_map.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
/// NCCH header (Note: "NCCH" appears to be a publically unknown acronym)

struct NCCH_Header {
    u8 signature[0x100];
    char magic[4];
    u32 content_size;
    u8 partition_id[8];
    u16 maker_code;
    u16 version;
    u8 reserved_0[4];
    u8 program_id[8];
    u8 temp_flag;
    u8 reserved_1[0x2f];
    u8 product_code[0x10];
    u8 extended_header_hash[0x20];
    u32 extended_header_size;
    u8 reserved_2[4];
    u8 flags[8];
    u32 plain_region_offset;
    u32 plain_region_size;
    u8 reserved_3[8];
    u32 exefs_offset;
    u32 exefs_size;
    u32 exefs_hash_region_size;
    u8 reserved_4[4];
    u32 romfs_offset;
    u32 romfs_size;
    u32 romfs_hash_region_size;
    u8 reserved_5[4];
    u8 exefs_super_block_hash[0x20];
    u8 romfs_super_block_hash[0x20];
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// ExeFS (executable file system) headers

typedef struct {
    char name[8];
    u32 offset;
    u32 size;
} ExeFs_SectionHeader;

typedef struct {
    ExeFs_SectionHeader section[8];
    u8 reserved[0x80];
    u8 hashes[8][0x20];
} ExeFs_Header;

////////////////////////////////////////////////////////////////////////////////////////////////////
// ExHeader (executable file system header) headers

struct ExHeader_SystemInfoFlags{
    u8 reserved[5];
    u8 flag;
    u8 remaster_version[2];
} exheader_systeminfoflags;

struct ExHeader_CodeSegmentInfo{
    u32 address;
    u32 num_max_pages;
    u32 code_size;
} exheader_codesegmentinfo;

struct ExHeader_CodeSetInfo {
    u8 name[8];
    ExHeader_SystemInfoFlags flags;
    ExHeader_CodeSegmentInfo text;
    u8 stacksize[4];
    ExHeader_CodeSegmentInfo ro;
    u8 reserved[4];
    ExHeader_CodeSegmentInfo data;
    u8 bsssize[4];
};

struct ExHeader_DependencyList{
    u8 program_id[0x30][8];
};

struct ExHeader_SystemInfo{
    u32 save_data_size;
    u8 reserved[4];
    u8 jump_id[8];
    u8 reserved_2[0x30];
};

struct ExHeader_StorageInfo{
    u8 ext_save_data_id[8];
    u8 system_save_data_id[8];
    u8 reserved[8];
    u8 access_info[7];
    u8 other_attributes;
};

struct ExHeader_ARM11_SystemLocalCaps{
    u8 program_id[8];
    u8 flags[8];
    u8 resource_limit_descriptor[0x10][2];
    ExHeader_StorageInfo storage_info;
    u8 service_access_control[0x20][8];
    u8 reserved[0x1f];
    u8 resource_limit_category;
};

struct ExHeader_ARM11_KernelCaps{
    u8 descriptors[28][4];
    u8 reserved[0x10];
};

struct ExHeader_ARM9_AccessControl{
    u8 descriptors[15];
    u8 descversion;
};

struct ExHeader_Header{
    ExHeader_CodeSetInfo codeset_info;
    ExHeader_DependencyList dependency_list;
    ExHeader_SystemInfo system_info;
    ExHeader_ARM11_SystemLocalCaps arm11_system_local_caps;
    ExHeader_ARM11_KernelCaps arm11_kernel_caps;
    ExHeader_ARM9_AccessControl arm9_access_control;
    struct {
        u8 signature[0x100];
        u8 ncch_public_key_modulus[0x100];
        ExHeader_ARM11_SystemLocalCaps arm11_system_local_caps;
        ExHeader_ARM11_KernelCaps arm11_kernel_caps;
        ExHeader_ARM9_AccessControl arm9_access_control;
    } access_desc;
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

const int kExeFs_MaxSections    = 8;        ///< Maximum number of sections (files) in an ExeFs
const int kExeFs_BlockSize      = 0x200;    ///< Size of ExeFS blocks (in bytes)

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
 * @param error_string String populated with error message on failure
 * @return True on success, otherwise false
 */
bool LZSS_Decompress(u8* compressed, u32 compressed_size, u8* decompressed, u32 decompressed_size, 
    std::string* error_string) {
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
                if(index < 2) {
                    *error_string = "Compression out of bounds";
                    return false;
                }
                index -= 2;

                u32 segment_offset = compressed[index] | (compressed[index + 1] << 8);
                u32 segment_size = ((segment_offset >> 12) & 15) + 3;
                segment_offset &= 0x0FFF;
                segment_offset += 2;

                if(out < segment_size) {
                    *error_string = "Compression out of bounds";
                    return false;
                }
                for(j = 0; j < segment_size; j++) {
                    u8 data;
                    if(out + segment_offset >= decompressed_size) {
                        *error_string = "Compression out of bounds";
                        return false;
                    }
                    data  = decompressed[out + segment_offset];
                    decompressed[--out] = data;
                }
            } else {
                if(out < 1) {
                    *error_string = "Compression out of bounds";
                    return false;
                }
                decompressed[--out] = compressed[--index];
            }
            control <<= 1;
        }
    }
    return true;
}

/**
 * Load a data buffer into memory at the specified address
 * @param addr Address to load memory into
 * @param buffer Buffer of data to load into memory
 * @param size Size of data to load into memory
 * @todo Perhaps move this code somewhere more generic?
 */
void LoadBuffer(const u32 addr, const u8* const buffer, const int size) {
    u32 *dst = (u32*)Memory::GetPointer(addr);
    u32 *src = (u32*)buffer;
    int size_aligned = (size + 3) / 4;

    for (int j = 0; j < size_aligned; j++) {
        *dst++ = (*src++);
    }
    return;
}

/**
 * Loads an NCCH file (e.g. from a CCI, or the first NCCH in a CXI)
 * @param filename String filename of NCCH file
 * @param error_string Pointer to string to put error message if an error has occurred
 * @todo Move NCSD parsing out of here and create a separate function for loading these
 * @return True on success, otherwise false
 */
bool Load_NCCH(std::string& filename, std::string* error_string) {
    INFO_LOG(LOADER, "Loading NCCH file %s...", filename.c_str());

    File::IOFile file(filename, "rb");

    if (file.IsOpen()) {
        NCCH_Header ncch_header;
        file.ReadBytes(&ncch_header, sizeof(NCCH_Header));

        // Skip NCSD header and load first NCCH (NCSD is just a container of NCCH files)...
        int ncch_off = 0; // Offset to NCCH header, can be 0 or after NCSD header
        if (memcmp(&ncch_header.magic, "NCSD", 4) == 0) {
            WARN_LOG(LOADER, "Only loading the first (bootable) NCCH within the NCSD file!");
            ncch_off = 0x4000;
            file.Seek(ncch_off, 0);
            file.ReadBytes(&ncch_header, sizeof(NCCH_Header));
        }
        // Verify we are loading the correct file type...
        if (memcmp(&ncch_header.magic, "NCCH", 4) != 0) {
            *error_string = "Invalid NCCH magic number (likely incorrect file type)";
            return false;
        }
        // Read ExHeader
        ExHeader_Header exheader_header;
        file.ReadBytes(&exheader_header, sizeof(ExHeader_Header));

        bool is_compressed = (exheader_header.codeset_info.flags.flag & 1) == 1;

        INFO_LOG(LOADER, "Name:            %s", exheader_header.codeset_info.name);
        INFO_LOG(LOADER, "Code compressed: %s", is_compressed ? "yes" : "no");

        // Read ExeFS
        u32 exefs_offset = ncch_header.exefs_offset * kExeFs_BlockSize;
        u32 exefs_size = ncch_header.exefs_size * kExeFs_BlockSize;

        INFO_LOG(LOADER, "ExeFS offset:    0x%08X", exefs_offset);
        INFO_LOG(LOADER, "ExeFS size:      0x%08X", exefs_size);

        ExeFs_Header exefs_header;
        file.Seek(exefs_offset + ncch_off, 0);
        file.ReadBytes(&exefs_header, sizeof(ExeFs_Header));

        // Iterate through the ExeFs archive until we find the .code file...
        for (int i = 0; i < kExeFs_MaxSections; i++) {
            INFO_LOG(LOADER, "ExeFS section %d:", i);
            INFO_LOG(LOADER, "    name:   %s", exefs_header.section[i].name);
            INFO_LOG(LOADER, "    offset: 0x%08X", exefs_header.section[i].offset);
            INFO_LOG(LOADER, "    size:   0x%08X", exefs_header.section[i].size);

            // Load the .code section (executable code)...
            if (strcmp((char*) exefs_header.section[i].name, ".code") == 0) {
                file.Seek(exefs_header.section[i].offset + exefs_offset + sizeof(ExeFs_Header) + 
                    ncch_off, 0);

                u8* buffer = new u8[exefs_header.section[i].size];
                file.ReadBytes(buffer, exefs_header.section[i].size);

                // Load compressed executable...
                if (i == 0 && is_compressed) {
                    u32 decompressed_size = LZSS_GetDecompressedSize(buffer, 
                        exefs_header.section[i].size);
                    u8* decompressed_buffer = new u8[decompressed_size];

                    if (!LZSS_Decompress(buffer, exefs_header.section[i].size, decompressed_buffer,
                        decompressed_size, error_string)) {
                        return false;
                    }
                    // Load .code section into memory...
                    LoadBuffer(exheader_header.codeset_info.text.address, decompressed_buffer,
                        decompressed_size);

                    delete[] decompressed_buffer;

                // Load uncompressed executable...
                } else {
                    // Load .code section into memory...
                    LoadBuffer(exheader_header.codeset_info.text.address, buffer, 
                        exefs_header.section[i].size);
                }
                delete[] buffer;

                // Setup kernel emulation to boot .code section...
                Kernel::LoadExec(exheader_header.codeset_info.text.address);

                // No need to load the other files from ExeFS until we do something with them...
                return true;
            }
        }
    }
    return false;
}

} // namespace Loader
