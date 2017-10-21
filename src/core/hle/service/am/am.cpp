// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <cinttypes>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/file_sys/ncch_container.h"
#include "core/file_sys/title_metadata.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/result.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/am_app.h"
#include "core/hle/service/am/am_net.h"
#include "core/hle/service/am/am_sys.h"
#include "core/hle/service/am/am_u.h"
#include "core/hle/service/fs/archive.h"
#include "core/hle/service/service.h"
#include "core/loader/loader.h"

namespace Service {
namespace AM {

constexpr u32 TID_HIGH_UPDATE = 0x0004000E;
constexpr u32 TID_HIGH_DLC = 0x0004008C;

static bool lists_initialized = false;
static std::array<std::vector<u64_le>, 3> am_title_list;

struct TitleInfo {
    u64_le tid;
    u64_le size;
    u16_le version;
    u16_le unused;
    u32_le type;
};

static_assert(sizeof(TitleInfo) == 0x18, "Title info structure size is wrong");

struct ContentInfo {
    u16_le index;
    u16_le type;
    u32_le content_id;
    u64_le size;
    u64_le romfs_size;
};

static_assert(sizeof(ContentInfo) == 0x18, "Content info structure size is wrong");

struct TicketInfo {
    u64_le title_id;
    u64_le ticket_id;
    u16_le version;
    u16_le unused;
    u32_le size;
};

static_assert(sizeof(TicketInfo) == 0x18, "Ticket info structure size is wrong");

std::string GetTitleMetadataPath(Service::FS::MediaType media_type, u64 tid) {
    std::string content_path = GetTitlePath(media_type, tid) + "content/";

    if (media_type == Service::FS::MediaType::GameCard) {
        LOG_ERROR(Service_AM, "Invalid request for nonexistent gamecard title metadata!");
        return "";
    }

    // The TMD ID is usually held in the title databases, which we don't implement.
    // For now, just scan for any .tmd files which exist and use the first .tmd
    // found (there should only really be one unless the directories are meddled with)
    FileUtil::FSTEntry entries;
    FileUtil::ScanDirectoryTree(content_path, entries);
    for (const FileUtil::FSTEntry& entry : entries.children) {
        std::string filename_filename, filename_extension;
        Common::SplitPath(entry.virtualName, nullptr, &filename_filename, &filename_extension);

        if (filename_extension == ".tmd")
            return content_path + entry.virtualName;
    }

    // If we can't find an existing .tmd, return a path for one to be created.
    return content_path + "00000000.tmd";
}

std::string GetTitleContentPath(Service::FS::MediaType media_type, u64 tid, u16 index) {
    std::string content_path = GetTitlePath(media_type, tid) + "content/";

    if (media_type == Service::FS::MediaType::GameCard) {
        // TODO(shinyquagsire23): get current app file if TID matches?
        LOG_ERROR(Service_AM, "Request for gamecard partition %u content path unimplemented!",
                  static_cast<u32>(index));
        return "";
    }

    std::string tmd_path = GetTitleMetadataPath(media_type, tid);

    u32 content_id = 0;
    FileSys::TitleMetadata tmd;
    if (tmd.LoadFromFile(tmd_path) == Loader::ResultStatus::Success) {
        content_id = tmd.GetContentIDByIndex(index);

        // TODO(shinyquagsire23): how does DLC actually get this folder on hardware?
        // For now, check if the second (index 1) content has the optional flag set, for most
        // apps this is usually the manual and not set optional, DLC has it set optional.
        // All .apps (including index 0) will be in the 00000000/ folder for DLC.
        if (tmd.GetContentCount() > 1 &&
            tmd.GetContentTypeByIndex(1) & FileSys::TMDContentTypeFlag::Optional) {
            content_path += "00000000/";
        }
    }

    return Common::StringFromFormat("%s%08x.app", content_path.c_str(), content_id);
}

std::string GetTitlePath(Service::FS::MediaType media_type, u64 tid) {
    u32 high = static_cast<u32>(tid >> 32);
    u32 low = static_cast<u32>(tid & 0xFFFFFFFF);

    if (media_type == Service::FS::MediaType::NAND || media_type == Service::FS::MediaType::SDMC)
        return Common::StringFromFormat("%s%08x/%08x/", GetMediaTitlePath(media_type).c_str(), high,
                                        low);

    if (media_type == Service::FS::MediaType::GameCard) {
        // TODO(shinyquagsire23): get current app path if TID matches?
        LOG_ERROR(Service_AM, "Request for gamecard title path unimplemented!");
        return "";
    }

    return "";
}

std::string GetMediaTitlePath(Service::FS::MediaType media_type) {
    if (media_type == Service::FS::MediaType::NAND)
        return Common::StringFromFormat("%s%s/title/", FileUtil::GetUserPath(D_NAND_IDX).c_str(),
                                        SYSTEM_ID);

    if (media_type == Service::FS::MediaType::SDMC)
        return Common::StringFromFormat("%sNintendo 3DS/%s/%s/title/",
                                        FileUtil::GetUserPath(D_SDMC_IDX).c_str(), SYSTEM_ID,
                                        SDCARD_ID);

    if (media_type == Service::FS::MediaType::GameCard) {
        // TODO(shinyquagsire23): get current app parent folder if TID matches?
        LOG_ERROR(Service_AM, "Request for gamecard parent path unimplemented!");
        return "";
    }

    return "";
}

void ScanForTitles(Service::FS::MediaType media_type) {
    am_title_list[static_cast<u32>(media_type)].clear();

    std::string title_path = GetMediaTitlePath(media_type);

    FileUtil::FSTEntry entries;
    FileUtil::ScanDirectoryTree(title_path, entries, 1);
    for (const FileUtil::FSTEntry& tid_high : entries.children) {
        for (const FileUtil::FSTEntry& tid_low : tid_high.children) {
            std::string tid_string = tid_high.virtualName + tid_low.virtualName;
            u64 tid = std::stoull(tid_string.c_str(), nullptr, 16);

            FileSys::NCCHContainer container(GetTitleContentPath(media_type, tid));
            if (container.Load() == Loader::ResultStatus::Success)
                am_title_list[static_cast<u32>(media_type)].push_back(tid);
        }
    }
}

void ScanForAllTitles() {
    ScanForTitles(Service::FS::MediaType::NAND);
    ScanForTitles(Service::FS::MediaType::SDMC);
}

void GetNumPrograms(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1, 1, 0); // 0x00010040
    u32 media_type = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(am_title_list[media_type].size());
}

void FindContentInfos(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1002, 4, 4); // 0x10020104

    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());
    u64 title_id = rp.Pop<u64>();
    u32 content_count = rp.Pop<u32>();
    VAddr content_requested_in = rp.PopMappedBuffer();
    VAddr content_info_out = rp.PopMappedBuffer();

    std::vector<u16_le> content_requested(content_count);
    Memory::ReadBlock(content_requested_in, content_requested.data(), content_count * sizeof(u16));

    std::string tmd_path = GetTitleMetadataPath(media_type, title_id);

    u32 content_read = 0;
    FileSys::TitleMetadata tmd;
    if (tmd.LoadFromFile(tmd_path) == Loader::ResultStatus::Success) {
        // Get info for each content index requested
        for (size_t i = 0; i < content_count; i++) {
            std::shared_ptr<FileUtil::IOFile> romfs_file;
            u64 romfs_offset = 0;
            u64 romfs_size = 0;

            FileSys::NCCHContainer ncch_container(GetTitleContentPath(media_type, title_id, i));
            ncch_container.ReadRomFS(romfs_file, romfs_offset, romfs_size);

            ContentInfo content_info = {};
            content_info.index = static_cast<u16>(i);
            content_info.type = tmd.GetContentTypeByIndex(content_requested[i]);
            content_info.content_id = tmd.GetContentIDByIndex(content_requested[i]);
            content_info.size = tmd.GetContentSizeByIndex(content_requested[i]);
            content_info.romfs_size = romfs_size;

            Memory::WriteBlock(content_info_out, &content_info, sizeof(ContentInfo));
            content_info_out += sizeof(ContentInfo);
            content_read++;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
}

void ListContentInfos(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1003, 5, 2); // 0x10030142
    u32 content_count = rp.Pop<u32>();
    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());
    u64 title_id = rp.Pop<u64>();
    u32 start_index = rp.Pop<u32>();
    VAddr content_info_out = rp.PopMappedBuffer();

    std::string tmd_path = GetTitleMetadataPath(media_type, title_id);

    u32 copied = 0;
    FileSys::TitleMetadata tmd;
    if (tmd.LoadFromFile(tmd_path) == Loader::ResultStatus::Success) {
        copied = std::min(content_count, static_cast<u32>(tmd.GetContentCount()));
        for (u32 i = start_index; i < copied; i++) {
            std::shared_ptr<FileUtil::IOFile> romfs_file;
            u64 romfs_offset = 0;
            u64 romfs_size = 0;

            FileSys::NCCHContainer ncch_container(GetTitleContentPath(media_type, title_id, i));
            ncch_container.ReadRomFS(romfs_file, romfs_offset, romfs_size);

            ContentInfo content_info = {};
            content_info.index = static_cast<u16>(i);
            content_info.type = tmd.GetContentTypeByIndex(i);
            content_info.content_id = tmd.GetContentIDByIndex(i);
            content_info.size = tmd.GetContentSizeByIndex(i);
            content_info.romfs_size = romfs_size;

            Memory::WriteBlock(content_info_out, &content_info, sizeof(ContentInfo));
            content_info_out += sizeof(ContentInfo);
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(copied);
}

void DeleteContents(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1004, 4, 2); // 0x10040102
    u8 media_type = rp.Pop<u8>();
    u64 title_id = rp.Pop<u64>();
    u32 content_count = rp.Pop<u32>();
    VAddr content_ids_in = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_AM, "(STUBBED) media_type=%u, title_id=0x%016" PRIx64
                            ", content_count=%u, content_ids_in=0x%08x",
                media_type, title_id, content_count, content_ids_in);
}

void GetProgramList(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 2, 2, 2); // 0x00020082

    u32 count = rp.Pop<u32>();
    u8 media_type = rp.Pop<u8>();
    VAddr title_ids_output_pointer = rp.PopMappedBuffer();

    if (!Memory::IsValidVirtualAddress(title_ids_output_pointer) || media_type > 2) {
        IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
        rb.Push<u32>(-1); // TODO(shinyquagsire23): Find the right error code
        rb.Push<u32>(0);
        return;
    }

    u32 media_count = static_cast<u32>(am_title_list[media_type].size());
    u32 copied = std::min(media_count, count);

    Memory::WriteBlock(title_ids_output_pointer, am_title_list[media_type].data(),
                       copied * sizeof(u64));

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(copied);
}

ResultCode GetTitleInfoFromList(const std::vector<u64>& title_id_list,
                                Service::FS::MediaType media_type, VAddr title_info_out) {
    for (u32 i = 0; i < title_id_list.size(); i++) {
        std::string tmd_path = GetTitleMetadataPath(media_type, title_id_list[i]);

        TitleInfo title_info = {};
        title_info.tid = title_id_list[i];

        FileSys::TitleMetadata tmd;
        if (tmd.LoadFromFile(tmd_path) == Loader::ResultStatus::Success) {
            // TODO(shinyquagsire23): This is the total size of all files this process owns,
            // including savefiles and other content. This comes close but is off.
            title_info.size = tmd.GetContentSizeByIndex(FileSys::TMDContentIndex::Main);
            title_info.version = tmd.GetTitleVersion();
            title_info.type = tmd.GetTitleType();
        } else {
            return ResultCode(ErrorDescription::NotFound, ErrorModule::AM,
                              ErrorSummary::InvalidState, ErrorLevel::Permanent);
        }
        Memory::WriteBlock(title_info_out, &title_info, sizeof(TitleInfo));
        title_info_out += sizeof(TitleInfo);
    }

    return RESULT_SUCCESS;
}

void GetProgramInfos(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 3, 2, 4); // 0x00030084

    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());
    u32 title_count = rp.Pop<u32>();

    size_t title_id_list_size, title_info_size;
    IPC::MappedBufferPermissions title_id_list_perms, title_info_perms;
    VAddr title_id_list_pointer = rp.PopMappedBuffer(&title_id_list_size, &title_id_list_perms);
    VAddr title_info_out = rp.PopMappedBuffer(&title_info_size, &title_info_perms);

    std::vector<u64> title_id_list(title_count);
    Memory::ReadBlock(title_id_list_pointer, title_id_list.data(), title_count * sizeof(u64));

    ResultCode result = GetTitleInfoFromList(title_id_list, media_type, title_info_out);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(result);
    rb.PushMappedBuffer(title_id_list_pointer, title_id_list_size, title_id_list_perms);
    rb.PushMappedBuffer(title_info_out, title_info_size, title_info_perms);
}

void GetDLCTitleInfos(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1005, 2, 4); // 0x10050084

    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());
    u32 title_count = rp.Pop<u32>();

    size_t title_id_list_size, title_info_size;
    IPC::MappedBufferPermissions title_id_list_perms, title_info_perms;
    VAddr title_id_list_pointer = rp.PopMappedBuffer(&title_id_list_size, &title_id_list_perms);
    VAddr title_info_out = rp.PopMappedBuffer(&title_info_size, &title_info_perms);

    std::vector<u64> title_id_list(title_count);
    Memory::ReadBlock(title_id_list_pointer, title_id_list.data(), title_count * sizeof(u64));

    ResultCode result = RESULT_SUCCESS;

    // Validate that DLC TIDs were passed in
    for (u32 i = 0; i < title_count; i++) {
        u32 tid_high = static_cast<u32>(title_id_list[i] >> 32);
        if (tid_high != TID_HIGH_DLC) {
            result = ResultCode(ErrCodes::InvalidTIDInList, ErrorModule::AM,
                                ErrorSummary::InvalidArgument, ErrorLevel::Usage);
            break;
        }
    }

    if (result.IsSuccess()) {
        result = GetTitleInfoFromList(title_id_list, media_type, title_info_out);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(result);
    rb.PushMappedBuffer(title_id_list_pointer, title_id_list_size, title_id_list_perms);
    rb.PushMappedBuffer(title_info_out, title_info_size, title_info_perms);
}

void GetPatchTitleInfos(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x100D, 2, 4); // 0x100D0084

    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());
    u32 title_count = rp.Pop<u32>();

    size_t title_id_list_size, title_info_size;
    IPC::MappedBufferPermissions title_id_list_perms, title_info_perms;
    VAddr title_id_list_pointer = rp.PopMappedBuffer(&title_id_list_size, &title_id_list_perms);
    VAddr title_info_out = rp.PopMappedBuffer(&title_info_size, &title_info_perms);

    std::vector<u64> title_id_list(title_count);
    Memory::ReadBlock(title_id_list_pointer, title_id_list.data(), title_count * sizeof(u64));

    ResultCode result = RESULT_SUCCESS;

    // Validate that update TIDs were passed in
    for (u32 i = 0; i < title_count; i++) {
        u32 tid_high = static_cast<u32>(title_id_list[i] >> 32);
        if (tid_high != TID_HIGH_UPDATE) {
            result = ResultCode(ErrCodes::InvalidTIDInList, ErrorModule::AM,
                                ErrorSummary::InvalidArgument, ErrorLevel::Usage);
            break;
        }
    }

    if (result.IsSuccess()) {
        result = GetTitleInfoFromList(title_id_list, media_type, title_info_out);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 4);
    rb.Push(result);
    rb.PushMappedBuffer(title_id_list_pointer, title_id_list_size, title_id_list_perms);
    rb.PushMappedBuffer(title_info_out, title_info_size, title_info_perms);
}

void ListDataTitleTicketInfos(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1007, 4, 4); // 0x10070102
    u32 ticket_count = rp.Pop<u32>();
    u64 title_id = rp.Pop<u64>();
    u32 start_index = rp.Pop<u32>();
    VAddr ticket_info_out = rp.PopMappedBuffer();
    VAddr ticket_info_write = ticket_info_out;

    for (u32 i = 0; i < ticket_count; i++) {
        TicketInfo ticket_info = {};
        ticket_info.title_id = title_id;
        ticket_info.version = 0; // TODO
        ticket_info.size = 0;    // TODO

        Memory::WriteBlock(ticket_info_write, &ticket_info, sizeof(TicketInfo));
        ticket_info_write += sizeof(TicketInfo);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(ticket_count);

    LOG_WARNING(Service_AM, "(STUBBED) ticket_count=0x%08X, title_id=0x%016" PRIx64
                            ", start_index=0x%08X, ticket_info_out=0x%08X",
                ticket_count, title_id, start_index, ticket_info_out);
}

void GetNumContentInfos(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1001, 3, 0); // 0x100100C0
    auto media_type = static_cast<Service::FS::MediaType>(rp.Pop<u8>());
    u64 title_id = rp.Pop<u64>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS); // No error

    std::string tmd_path = GetTitleMetadataPath(media_type, title_id);

    FileSys::TitleMetadata tmd;
    if (tmd.LoadFromFile(tmd_path) == Loader::ResultStatus::Success)
        rb.Push<u32>(tmd.GetContentCount());
    } else {
        rb.Push<u32>(1); // Number of content infos plus one
        LOG_WARNING(Service_AM, "(STUBBED) called media_type=%u, title_id=0x%016" PRIx64,
                    media_type, title_id);
    }
}

void DeleteTicket(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 7, 2, 0); // 0x00070080
    u64 title_id = rp.Pop<u64>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_AM, "(STUBBED) called title_id=0x%016" PRIx64 "", title_id);
}

void GetNumTickets(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 8, 0, 0); // 0x00080000
    u32 ticket_count = 0;

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(ticket_count);
    LOG_WARNING(Service_AM, "(STUBBED) called ticket_count=0x%08x", ticket_count);
}

void GetTicketList(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 9, 2, 2); // 0x00090082
    u32 ticket_list_count = rp.Pop<u32>();
    u32 ticket_index = rp.Pop<u32>();
    VAddr ticket_tids_out = rp.PopMappedBuffer();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(ticket_list_count);
    LOG_WARNING(Service_AM,
                "(STUBBED) ticket_list_count=0x%08x, ticket_index=0x%08x, ticket_tids_out=0x%08x",
                ticket_list_count, ticket_index, ticket_tids_out);
}

void QueryAvailableTitleDatabase(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x19, 1, 0); // 0x190040
    u8 media_type = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(true);

    LOG_WARNING(Service_APT, "(STUBBED) media_type=%u", media_type);
}

void CheckContentRights(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x25, 3, 0); // 0x2500C0
    u64 tid = rp.Pop<u64>();
    u16 content_index = rp.Pop<u16>();

    // TODO(shinyquagsire23): Read tickets for this instead?
    bool has_rights =
        FileUtil::Exists(GetTitleContentPath(Service::FS::MediaType::SDMC, tid, content_index));

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(has_rights);

    LOG_WARNING(Service_APT, "(STUBBED) tid=%016" PRIx64 ", content_index=%u", tid, content_index);
}

void CheckContentRightsIgnorePlatform(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x2D, 3, 0); // 0x2D00C0
    u64 tid = rp.Pop<u64>();
    u16 content_index = rp.Pop<u16>();

    // TODO(shinyquagsire23): Read tickets for this instead?
    bool has_rights =
        FileUtil::Exists(GetTitleContentPath(Service::FS::MediaType::SDMC, tid, content_index));

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS); // No error
    rb.Push(has_rights);

    LOG_WARNING(Service_APT, "(STUBBED) tid=%016" PRIx64 ", content_index=%u", tid, content_index);
}

void Init() {
    AddService(new AM_APP_Interface);
    AddService(new AM_NET_Interface);
    AddService(new AM_SYS_Interface);
    AddService(new AM_U_Interface);

    ScanForAllTitles();
}

void Shutdown() {}

} // namespace AM

} // namespace Service
