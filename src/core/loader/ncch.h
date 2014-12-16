// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common.h"
#include "common/file_util.h"

#include "core/loader/loader.h"

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

struct ExeFs_SectionHeader {
    char name[8];
    u32 offset;
    u32 size;
};

struct ExeFs_Header {
    ExeFs_SectionHeader section[8];
    u8 reserved[0x80];
    u8 hashes[8][0x20];
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// ExHeader (executable file system header) headers

struct ExHeader_SystemInfoFlags{
    u8 reserved[5];
    u8 flag;
    u8 remaster_version[2];
};

struct ExHeader_CodeSegmentInfo{
    u32 address;
    u32 num_max_pages;
    u32 code_size;
};

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

/// Loads an NCCH file (e.g. from a CCI, or the first NCCH in a CXI)
class AppLoader_NCCH final : public AppLoader {
public:
    AppLoader_NCCH(const std::string& filename);
    ~AppLoader_NCCH() override;

    /**
     * Load the application
     * @return ResultStatus result of function
     */
    ResultStatus Load() override;

    /**
     * Get the code (typically .code section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    ResultStatus ReadCode(std::vector<u8>& buffer) const override;

    /**
     * Get the icon (typically icon section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    ResultStatus ReadIcon(std::vector<u8>& buffer) const override;

    /**
     * Get the banner (typically banner section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    ResultStatus ReadBanner(std::vector<u8>& buffer) const override;

    /**
     * Get the logo (typically logo section) of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    ResultStatus ReadLogo(std::vector<u8>& buffer) const override;

    /**
     * Get the RomFS of the application
     * @param buffer Reference to buffer to store data
     * @return ResultStatus result of function
     */
    ResultStatus ReadRomFS(std::vector<u8>& buffer) const override;

    /*
     * Gets the program id from the NCCH header
     * @return u64 Program id
     */
    u64 GetProgramId() const;

private:

    /**
     * Reads an application ExeFS section of an NCCH file into AppLoader (e.g. .code, .logo, etc.)
     * @param name Name of section to read out of NCCH file
     * @param buffer Vector to read data into
     * @return ResultStatus result of function
     */
    ResultStatus LoadSectionExeFS(const char* name, std::vector<u8>& buffer) const;

    /**
     * Loads .code section into memory for booting
     * @return ResultStatus result of function
     */
    ResultStatus LoadExec() const;

    std::string     filename;

    bool            is_loaded;
    bool            is_compressed;

    u32             entry_point;
    u32             ncch_offset; // Offset to NCCH header, can be 0 or after NCSD header
    u32             exefs_offset;

    NCCH_Header     ncch_header;
    ExeFs_Header    exefs_header;
    ExHeader_Header exheader_header;
};

} // namespace Loader
