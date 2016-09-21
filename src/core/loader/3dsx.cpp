// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <vector>
#include "common/logging/log.h"
#include "core/file_sys/archive_romfs.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/service/fs/archive.h"
#include "core/loader/3dsx.h"
#include "core/memory.h"

namespace Loader {

/*
 * File layout:
 * - File header
 * - Code, rodata and data relocation table headers
 * - Code segment
 * - Rodata segment
 * - Loadable (non-BSS) part of the data segment
 * - Code relocation table
 * - Rodata relocation table
 * - Data relocation table
 *
 * Memory layout before relocations are applied:
 * [0..codeSegSize)             -> code segment
 * [codeSegSize..rodataSegSize) -> rodata segment
 * [rodataSegSize..dataSegSize) -> data segment
 *
 * Memory layout after relocations are applied: well, however the loader sets it up :)
 * The entrypoint is always the start of the code segment.
 * The BSS section must be cleared manually by the application.
 */

enum THREEDSX_Error { ERROR_NONE = 0, ERROR_READ = 1, ERROR_FILE = 2, ERROR_ALLOC = 3 };

static const u32 RELOCBUFSIZE = 512;
static const unsigned int NUM_SEGMENTS = 3;

// File header
#pragma pack(1)
struct THREEDSX_Header {
    u32 magic;
    u16 header_size, reloc_hdr_size;
    u32 format_ver;
    u32 flags;

    // Sizes of the code, rodata and data segments +
    // size of the BSS section (uninitialized latter half of the data segment)
    u32 code_seg_size, rodata_seg_size, data_seg_size, bss_size;
    // offset and size of smdh
    u32 smdh_offset, smdh_size;
    // offset to filesystem
    u32 fs_offset;
};

// Relocation header: all fields (even extra unknown fields) are guaranteed to be relocation counts.
struct THREEDSX_RelocHdr {
    // # of absolute relocations (that is, fix address to post-relocation memory layout)
    u32 cross_segment_absolute;
    // # of cross-segment relative relocations (that is, 32bit signed offsets that need to be
    // patched)
    u32 cross_segment_relative;
    // more?

    // Relocations are written in this order:
    // - Absolute relocations
    // - Relative relocations
};

// Relocation entry: from the current pointer, skip X words and patch Y words
struct THREEDSX_Reloc {
    u16 skip, patch;
};
#pragma pack()

struct THREEloadinfo {
    u8* seg_ptrs[3]; // code, rodata & data
    u32 seg_addrs[3];
    u32 seg_sizes[3];
};

static u32 TranslateAddr(u32 addr, const THREEloadinfo* loadinfo, u32* offsets) {
    if (addr < offsets[0])
        return loadinfo->seg_addrs[0] + addr;
    if (addr < offsets[1])
        return loadinfo->seg_addrs[1] + addr - offsets[0];
    return loadinfo->seg_addrs[2] + addr - offsets[1];
}

using Kernel::SharedPtr;
using Kernel::CodeSet;

static THREEDSX_Error Load3DSXFile(FileUtil::IOFile& file, u32 base_addr,
                                   SharedPtr<CodeSet>* out_codeset) {
    if (!file.IsOpen())
        return ERROR_FILE;

    // Reset read pointer in case this file has been read before.
    file.Seek(0, SEEK_SET);

    THREEDSX_Header hdr;
    if (file.ReadBytes(&hdr, sizeof(hdr)) != sizeof(hdr))
        return ERROR_READ;

    THREEloadinfo loadinfo;
    // loadinfo segments must be a multiple of 0x1000
    loadinfo.seg_sizes[0] = (hdr.code_seg_size + 0xFFF) & ~0xFFF;
    loadinfo.seg_sizes[1] = (hdr.rodata_seg_size + 0xFFF) & ~0xFFF;
    loadinfo.seg_sizes[2] = (hdr.data_seg_size + 0xFFF) & ~0xFFF;
    u32 offsets[2] = {loadinfo.seg_sizes[0], loadinfo.seg_sizes[0] + loadinfo.seg_sizes[1]};
    u32 n_reloc_tables = hdr.reloc_hdr_size / sizeof(u32);
    std::vector<u8> program_image(loadinfo.seg_sizes[0] + loadinfo.seg_sizes[1] +
                                  loadinfo.seg_sizes[2]);

    loadinfo.seg_addrs[0] = base_addr;
    loadinfo.seg_addrs[1] = loadinfo.seg_addrs[0] + loadinfo.seg_sizes[0];
    loadinfo.seg_addrs[2] = loadinfo.seg_addrs[1] + loadinfo.seg_sizes[1];
    loadinfo.seg_ptrs[0] = program_image.data();
    loadinfo.seg_ptrs[1] = loadinfo.seg_ptrs[0] + loadinfo.seg_sizes[0];
    loadinfo.seg_ptrs[2] = loadinfo.seg_ptrs[1] + loadinfo.seg_sizes[1];

    // Skip header for future compatibility
    file.Seek(hdr.header_size, SEEK_SET);

    // Read the relocation headers
    std::vector<u32> relocs(n_reloc_tables * NUM_SEGMENTS);
    for (unsigned int current_segment = 0; current_segment < NUM_SEGMENTS; ++current_segment) {
        size_t size = n_reloc_tables * sizeof(u32);
        if (file.ReadBytes(&relocs[current_segment * n_reloc_tables], size) != size)
            return ERROR_READ;
    }

    // Read the segments
    if (file.ReadBytes(loadinfo.seg_ptrs[0], hdr.code_seg_size) != hdr.code_seg_size)
        return ERROR_READ;
    if (file.ReadBytes(loadinfo.seg_ptrs[1], hdr.rodata_seg_size) != hdr.rodata_seg_size)
        return ERROR_READ;
    if (file.ReadBytes(loadinfo.seg_ptrs[2], hdr.data_seg_size - hdr.bss_size) !=
        hdr.data_seg_size - hdr.bss_size)
        return ERROR_READ;

    // BSS clear
    memset((char*)loadinfo.seg_ptrs[2] + hdr.data_seg_size - hdr.bss_size, 0, hdr.bss_size);

    // Relocate the segments
    for (unsigned int current_segment = 0; current_segment < NUM_SEGMENTS; ++current_segment) {
        for (unsigned current_segment_reloc_table = 0; current_segment_reloc_table < n_reloc_tables;
             current_segment_reloc_table++) {
            u32 n_relocs = relocs[current_segment * n_reloc_tables + current_segment_reloc_table];
            if (current_segment_reloc_table >= 2) {
                // We are not using this table - ignore it because we don't know what it dose
                file.Seek(n_relocs * sizeof(THREEDSX_Reloc), SEEK_CUR);
                continue;
            }
            THREEDSX_Reloc reloc_table[RELOCBUFSIZE];

            u32* pos = (u32*)loadinfo.seg_ptrs[current_segment];
            const u32* end_pos = pos + (loadinfo.seg_sizes[current_segment] / 4);

            while (n_relocs) {
                u32 remaining = std::min(RELOCBUFSIZE, n_relocs);
                n_relocs -= remaining;

                if (file.ReadBytes(reloc_table, remaining * sizeof(THREEDSX_Reloc)) !=
                    remaining * sizeof(THREEDSX_Reloc))
                    return ERROR_READ;

                for (unsigned current_inprogress = 0;
                     current_inprogress < remaining && pos < end_pos; current_inprogress++) {
                    const auto& table = reloc_table[current_inprogress];
                    LOG_TRACE(Loader, "(t=%d,skip=%u,patch=%u)", current_segment_reloc_table,
                              static_cast<u32>(table.skip), static_cast<u32>(table.patch));
                    pos += table.skip;
                    s32 num_patches = table.patch;
                    while (0 < num_patches && pos < end_pos) {
                        u32 in_addr =
                            static_cast<u32>(reinterpret_cast<u8*>(pos) - program_image.data());
                        u32 addr = TranslateAddr(*pos, &loadinfo, offsets);
                        LOG_TRACE(Loader, "Patching %08X <-- rel(%08X,%d) (%08X)",
                                  base_addr + in_addr, addr, current_segment_reloc_table, *pos);
                        switch (current_segment_reloc_table) {
                        case 0:
                            *pos = (addr);
                            break;
                        case 1:
                            *pos = static_cast<u32>(addr - in_addr);
                            break;
                        default:
                            break; // this should never happen
                        }
                        pos++;
                        num_patches--;
                    }
                }
            }
        }
    }

    // Create the CodeSet
    SharedPtr<CodeSet> code_set = CodeSet::Create("", 0);

    code_set->code.offset = loadinfo.seg_ptrs[0] - program_image.data();
    code_set->code.addr = loadinfo.seg_addrs[0];
    code_set->code.size = loadinfo.seg_sizes[0];

    code_set->rodata.offset = loadinfo.seg_ptrs[1] - program_image.data();
    code_set->rodata.addr = loadinfo.seg_addrs[1];
    code_set->rodata.size = loadinfo.seg_sizes[1];

    code_set->data.offset = loadinfo.seg_ptrs[2] - program_image.data();
    code_set->data.addr = loadinfo.seg_addrs[2];
    code_set->data.size = loadinfo.seg_sizes[2];

    code_set->entrypoint = code_set->code.addr;
    code_set->memory = std::make_shared<std::vector<u8>>(std::move(program_image));

    LOG_DEBUG(Loader, "code size:   0x%X", loadinfo.seg_sizes[0]);
    LOG_DEBUG(Loader, "rodata size: 0x%X", loadinfo.seg_sizes[1]);
    LOG_DEBUG(Loader, "data size:   0x%X (including 0x%X of bss)", loadinfo.seg_sizes[2],
              hdr.bss_size);

    *out_codeset = code_set;
    return ERROR_NONE;
}

FileType AppLoader_THREEDSX::IdentifyType(FileUtil::IOFile& file) {
    u32 magic;
    file.Seek(0, SEEK_SET);
    if (1 != file.ReadArray<u32>(&magic, 1))
        return FileType::Error;

    if (MakeMagic('3', 'D', 'S', 'X') == magic)
        return FileType::THREEDSX;

    return FileType::Error;
}

ResultStatus AppLoader_THREEDSX::Load() {
    if (is_loaded)
        return ResultStatus::ErrorAlreadyLoaded;

    if (!file.IsOpen())
        return ResultStatus::Error;

    SharedPtr<CodeSet> codeset;
    if (Load3DSXFile(file, Memory::PROCESS_IMAGE_VADDR, &codeset) != ERROR_NONE)
        return ResultStatus::Error;
    codeset->name = filename;

    Kernel::g_current_process = Kernel::Process::Create(std::move(codeset));
    Kernel::g_current_process->svc_access_mask.set();
    Kernel::g_current_process->address_mappings = default_address_mappings;

    // Attach the default resource limit (APPLICATION) to the process
    Kernel::g_current_process->resource_limit =
        Kernel::ResourceLimit::GetForCategory(Kernel::ResourceLimitCategory::APPLICATION);

    Kernel::g_current_process->Run(48, Kernel::DEFAULT_STACK_SIZE);

    Service::FS::RegisterArchiveType(std::make_unique<FileSys::ArchiveFactory_RomFS>(*this),
                                     Service::FS::ArchiveIdCode::RomFS);

    is_loaded = true;
    return ResultStatus::Success;
}

ResultStatus AppLoader_THREEDSX::ReadRomFS(std::shared_ptr<FileUtil::IOFile>& romfs_file,
                                           u64& offset, u64& size) {
    if (!file.IsOpen())
        return ResultStatus::Error;

    // Reset read pointer in case this file has been read before.
    file.Seek(0, SEEK_SET);

    THREEDSX_Header hdr;
    if (file.ReadBytes(&hdr, sizeof(THREEDSX_Header)) != sizeof(THREEDSX_Header))
        return ResultStatus::Error;

    if (hdr.header_size != sizeof(THREEDSX_Header))
        return ResultStatus::Error;

    // Check if the 3DSX has a RomFS...
    if (hdr.fs_offset != 0) {
        u32 romfs_offset = hdr.fs_offset;
        u32 romfs_size = static_cast<u32>(file.GetSize()) - hdr.fs_offset;

        LOG_DEBUG(Loader, "RomFS offset:           0x%08X", romfs_offset);
        LOG_DEBUG(Loader, "RomFS size:             0x%08X", romfs_size);

        // We reopen the file, to allow its position to be independent from file's
        romfs_file = std::make_shared<FileUtil::IOFile>(filepath, "rb");
        if (!romfs_file->IsOpen())
            return ResultStatus::Error;

        offset = romfs_offset;
        size = romfs_size;

        return ResultStatus::Success;
    }
    LOG_DEBUG(Loader, "3DSX has no RomFS");
    return ResultStatus::ErrorNotUsed;
}

ResultStatus AppLoader_THREEDSX::ReadIcon(std::vector<u8>& buffer) {
    if (!file.IsOpen())
        return ResultStatus::Error;

    // Reset read pointer in case this file has been read before.
    file.Seek(0, SEEK_SET);

    THREEDSX_Header hdr;
    if (file.ReadBytes(&hdr, sizeof(THREEDSX_Header)) != sizeof(THREEDSX_Header))
        return ResultStatus::Error;

    if (hdr.header_size != sizeof(THREEDSX_Header))
        return ResultStatus::Error;

    // Check if the 3DSX has a SMDH...
    if (hdr.smdh_offset != 0) {
        file.Seek(hdr.smdh_offset, SEEK_SET);
        buffer.resize(hdr.smdh_size);

        if (file.ReadBytes(&buffer[0], hdr.smdh_size) != hdr.smdh_size)
            return ResultStatus::Error;

        return ResultStatus::Success;
    }
    return ResultStatus::ErrorNotUsed;
}

} // namespace Loader
