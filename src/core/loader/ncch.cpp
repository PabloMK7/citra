// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <codecvt>
#include <cstring>
#include <locale>
#include <memory>
#include <fmt/format.h>
#include "common/logging/log.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "core/core.h"
#include "core/file_sys/archive_selfncch.h"
#include "core/file_sys/ncch_container.h"
#include "core/file_sys/title_metadata.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/fs/archive.h"
#include "core/loader/ncch.h"
#include "core/loader/smdh.h"
#include "core/memory.h"
#include "network/network.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Loader namespace

namespace Loader {

static const u64 UPDATE_MASK = 0x0000000e00000000;

FileType AppLoader_NCCH::IdentifyType(FileUtil::IOFile& file) {
    u32 magic;
    file.Seek(0x100, SEEK_SET);
    if (1 != file.ReadArray<u32>(&magic, 1))
        return FileType::Error;

    if (MakeMagic('N', 'C', 'S', 'D') == magic)
        return FileType::CCI;

    if (MakeMagic('N', 'C', 'C', 'H') == magic)
        return FileType::CXI;

    return FileType::Error;
}

std::pair<boost::optional<u32>, ResultStatus> AppLoader_NCCH::LoadKernelSystemMode() {
    if (!is_loaded) {
        ResultStatus res = base_ncch.Load();
        if (res != ResultStatus::Success) {
            return std::make_pair(boost::none, res);
        }
    }

    // Set the system mode as the one from the exheader.
    return std::make_pair(overlay_ncch->exheader_header.arm11_system_local_caps.system_mode.Value(),
                          ResultStatus::Success);
}

ResultStatus AppLoader_NCCH::LoadExec(Kernel::SharedPtr<Kernel::Process>& process) {
    using Kernel::CodeSet;
    using Kernel::SharedPtr;

    if (!is_loaded)
        return ResultStatus::ErrorNotLoaded;

    std::vector<u8> code;
    u64_le program_id;
    if (ResultStatus::Success == ReadCode(code) &&
        ResultStatus::Success == ReadProgramId(program_id)) {
        std::string process_name = Common::StringFromFixedZeroTerminatedBuffer(
            (const char*)overlay_ncch->exheader_header.codeset_info.name, 8);

        SharedPtr<CodeSet> codeset = CodeSet::Create(process_name, program_id);

        codeset->CodeSegment().offset = 0;
        codeset->CodeSegment().addr = overlay_ncch->exheader_header.codeset_info.text.address;
        codeset->CodeSegment().size =
            overlay_ncch->exheader_header.codeset_info.text.num_max_pages * Memory::PAGE_SIZE;

        codeset->RODataSegment().offset =
            codeset->CodeSegment().offset + codeset->CodeSegment().size;
        codeset->RODataSegment().addr = overlay_ncch->exheader_header.codeset_info.ro.address;
        codeset->RODataSegment().size =
            overlay_ncch->exheader_header.codeset_info.ro.num_max_pages * Memory::PAGE_SIZE;

        // TODO(yuriks): Not sure if the bss size is added to the page-aligned .data size or just
        //               to the regular size. Playing it safe for now.
        u32 bss_page_size = (overlay_ncch->exheader_header.codeset_info.bss_size + 0xFFF) & ~0xFFF;
        code.resize(code.size() + bss_page_size, 0);

        codeset->DataSegment().offset =
            codeset->RODataSegment().offset + codeset->RODataSegment().size;
        codeset->DataSegment().addr = overlay_ncch->exheader_header.codeset_info.data.address;
        codeset->DataSegment().size =
            overlay_ncch->exheader_header.codeset_info.data.num_max_pages * Memory::PAGE_SIZE +
            bss_page_size;

        codeset->entrypoint = codeset->CodeSegment().addr;
        codeset->memory = std::make_shared<std::vector<u8>>(std::move(code));

        process = Kernel::Process::Create(std::move(codeset));

        // Attach a resource limit to the process based on the resource limit category
        process->resource_limit =
            Kernel::ResourceLimit::GetForCategory(static_cast<Kernel::ResourceLimitCategory>(
                overlay_ncch->exheader_header.arm11_system_local_caps.resource_limit_category));

        // Set the default CPU core for this process
        process->ideal_processor =
            overlay_ncch->exheader_header.arm11_system_local_caps.ideal_processor;

        // Copy data while converting endianness
        std::array<u32, ARRAY_SIZE(overlay_ncch->exheader_header.arm11_kernel_caps.descriptors)>
            kernel_caps;
        std::copy_n(overlay_ncch->exheader_header.arm11_kernel_caps.descriptors, kernel_caps.size(),
                    begin(kernel_caps));
        process->ParseKernelCaps(kernel_caps.data(), kernel_caps.size());

        s32 priority = overlay_ncch->exheader_header.arm11_system_local_caps.priority;
        u32 stack_size = overlay_ncch->exheader_header.codeset_info.stack_size;
        process->Run(priority, stack_size);
        return ResultStatus::Success;
    }
    return ResultStatus::Error;
}

void AppLoader_NCCH::ParseRegionLockoutInfo() {
    std::vector<u8> smdh_buffer;
    if (ReadIcon(smdh_buffer) == ResultStatus::Success && smdh_buffer.size() >= sizeof(SMDH)) {
        SMDH smdh;
        memcpy(&smdh, smdh_buffer.data(), sizeof(SMDH));
        u32 region_lockout = smdh.region_lockout;
        constexpr u32 REGION_COUNT = 7;
        for (u32 region = 0; region < REGION_COUNT; ++region) {
            if (region_lockout & 1) {
                Service::CFG::GetCurrentModule()->SetPreferredRegionCode(region);
                break;
            }
            region_lockout >>= 1;
        }
    }
}

ResultStatus AppLoader_NCCH::Load(Kernel::SharedPtr<Kernel::Process>& process) {
    u64_le ncch_program_id;

    if (is_loaded)
        return ResultStatus::ErrorAlreadyLoaded;

    ResultStatus result = base_ncch.Load();
    if (result != ResultStatus::Success)
        return result;

    ReadProgramId(ncch_program_id);
    std::string program_id{fmt::format("{:016X}", ncch_program_id)};

    LOG_INFO(Loader, "Program ID: {}", program_id);

    update_ncch.OpenFile(Service::AM::GetTitleContentPath(Service::FS::MediaType::SDMC,
                                                          ncch_program_id | UPDATE_MASK));
    result = update_ncch.Load();
    if (result == ResultStatus::Success) {
        overlay_ncch = &update_ncch;
    }

    Core::Telemetry().AddField(Telemetry::FieldType::Session, "ProgramId", program_id);

    if (auto room_member = Network::GetRoomMember().lock()) {
        Network::GameInfo game_info;
        ReadTitle(game_info.name);
        game_info.id = ncch_program_id;
        room_member->SendGameInfo(game_info);
    }

    is_loaded = true; // Set state to loaded

    result = LoadExec(process); // Load the executable into memory for booting
    if (ResultStatus::Success != result)
        return result;

    Service::FS::RegisterSelfNCCH(*this);

    ParseRegionLockoutInfo();

    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::ReadCode(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS(".code", buffer);
}

ResultStatus AppLoader_NCCH::ReadIcon(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS("icon", buffer);
}

ResultStatus AppLoader_NCCH::ReadBanner(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS("banner", buffer);
}

ResultStatus AppLoader_NCCH::ReadLogo(std::vector<u8>& buffer) {
    return overlay_ncch->LoadSectionExeFS("logo", buffer);
}

ResultStatus AppLoader_NCCH::ReadProgramId(u64& out_program_id) {
    ResultStatus result = base_ncch.ReadProgramId(out_program_id);
    if (result != ResultStatus::Success)
        return result;

    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::ReadRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
    return base_ncch.ReadRomFS(romfs_file);
}

ResultStatus AppLoader_NCCH::ReadUpdateRomFS(std::shared_ptr<FileSys::RomFSReader>& romfs_file) {
    ResultStatus result = update_ncch.ReadRomFS(romfs_file);

    if (result != ResultStatus::Success)
        return base_ncch.ReadRomFS(romfs_file);

    return ResultStatus::Success;
}

ResultStatus AppLoader_NCCH::ReadTitle(std::string& title) {
    std::vector<u8> data;
    Loader::SMDH smdh;
    ReadIcon(data);

    if (!Loader::IsValidSMDH(data)) {
        return ResultStatus::ErrorInvalidFormat;
    }

    memcpy(&smdh, data.data(), sizeof(Loader::SMDH));

    const auto& short_title = smdh.GetShortTitle(SMDH::TitleLanguage::English);
    auto title_end = std::find(short_title.begin(), short_title.end(), u'\0');
    title = Common::UTF16ToUTF8(std::u16string{short_title.begin(), title_end});

    return ResultStatus::Success;
}

} // namespace Loader
