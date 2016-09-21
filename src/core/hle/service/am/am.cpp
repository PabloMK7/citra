// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cinttypes>
#include "common/logging/log.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/am_app.h"
#include "core/hle/service/am/am_net.h"
#include "core/hle/service/am/am_sys.h"
#include "core/hle/service/am/am_u.h"
#include "core/hle/service/service.h"

namespace Service {
namespace AM {

static std::array<u32, 3> am_content_count = {0, 0, 0};
static std::array<u32, 3> am_titles_count = {0, 0, 0};
static std::array<u32, 3> am_titles_list_count = {0, 0, 0};
static u32 am_ticket_count = 0;
static u32 am_ticket_list_count = 0;

void GetTitleCount(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 media_type = cmd_buff[1] & 0xFF;

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = am_titles_count[media_type];
    LOG_WARNING(Service_AM, "(STUBBED) media_type=%u, title_count=0x%08x", media_type,
                am_titles_count[media_type]);
}

void FindContentInfos(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 media_type = cmd_buff[1] & 0xFF;
    u64 title_id = (static_cast<u64>(cmd_buff[3]) << 32) | cmd_buff[2];
    u32 content_ids_pointer = cmd_buff[6];
    u32 content_info_pointer = cmd_buff[8];

    am_content_count[media_type] = cmd_buff[4];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_AM, "(STUBBED) media_type=%u, title_id=0x%016llx, content_cound=%u, "
                            "content_ids_pointer=0x%08x, content_info_pointer=0x%08x",
                media_type, title_id, am_content_count[media_type], content_ids_pointer,
                content_info_pointer);
}

void ListContentInfos(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 media_type = cmd_buff[2] & 0xFF;
    u64 title_id = (static_cast<u64>(cmd_buff[4]) << 32) | cmd_buff[3];
    u32 start_index = cmd_buff[5];
    u32 content_info_pointer = cmd_buff[7];

    am_content_count[media_type] = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = am_content_count[media_type];
    LOG_WARNING(Service_AM, "(STUBBED) media_type=%u, content_count=%u, title_id=0x%016" PRIx64
                            ", start_index=0x%08x, content_info_pointer=0x%08X",
                media_type, am_content_count[media_type], title_id, start_index,
                content_info_pointer);
}

void DeleteContents(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 media_type = cmd_buff[1] & 0xFF;
    u64 title_id = (static_cast<u64>(cmd_buff[3]) << 32) | cmd_buff[2];
    u32 content_ids_pointer = cmd_buff[6];

    am_content_count[media_type] = cmd_buff[4];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_AM, "(STUBBED) media_type=%u, title_id=0x%016" PRIx64
                            ", content_count=%u, content_ids_pointer=0x%08x",
                media_type, title_id, am_content_count[media_type], content_ids_pointer);
}

void GetTitleList(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 media_type = cmd_buff[2] & 0xFF;
    u32 title_ids_output_pointer = cmd_buff[4];

    am_titles_list_count[media_type] = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = am_titles_list_count[media_type];
    LOG_WARNING(
        Service_AM,
        "(STUBBED) media_type=%u, titles_list_count=0x%08X, title_ids_output_pointer=0x%08X",
        media_type, am_titles_list_count[media_type], title_ids_output_pointer);
}

void GetTitleInfo(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 media_type = cmd_buff[1] & 0xFF;
    u32 title_id_list_pointer = cmd_buff[4];
    u32 title_list_pointer = cmd_buff[6];

    am_titles_count[media_type] = cmd_buff[2];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_AM, "(STUBBED) media_type=%u, total_titles=0x%08X, "
                            "title_id_list_pointer=0x%08X, title_list_pointer=0x%08X",
                media_type, am_titles_count[media_type], title_id_list_pointer, title_list_pointer);
}

void GetDataTitleInfos(Service::Interface* self) {
    GetTitleInfo(self);

    LOG_WARNING(Service_AM, "(STUBBED) called");
}

void ListDataTitleTicketInfos(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u64 title_id = (static_cast<u64>(cmd_buff[3]) << 32) | cmd_buff[2];
    u32 start_index = cmd_buff[4];
    u32 ticket_info_pointer = cmd_buff[6];

    am_ticket_count = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = am_ticket_count;
    LOG_WARNING(Service_AM, "(STUBBED) ticket_count=0x%08X, title_id=0x%016" PRIx64
                            ", start_index=0x%08X, ticket_info_pointer=0x%08X",
                am_ticket_count, title_id, start_index, ticket_info_pointer);
}

void GetNumContentInfos(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 1; // Number of content infos plus one
    LOG_WARNING(Service_AM, "(STUBBED) called");
}

void DeleteTicket(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u64 title_id = (static_cast<u64>(cmd_buff[2]) << 32) | cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_WARNING(Service_AM, "(STUBBED) called title_id=0x%016" PRIx64 "", title_id);
}

void GetTicketCount(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = am_ticket_count;
    LOG_WARNING(Service_AM, "(STUBBED) called ticket_count=0x%08x", am_ticket_count);
}

void GetTicketList(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 num_of_skip = cmd_buff[2];
    u32 ticket_list_pointer = cmd_buff[4];

    am_ticket_list_count = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = am_ticket_list_count;
    LOG_WARNING(
        Service_AM,
        "(STUBBED) ticket_list_count=0x%08x, num_of_skip=0x%08x, ticket_list_pointer=0x%08x",
        am_ticket_list_count, num_of_skip, ticket_list_pointer);
}

void Init() {
    using namespace Kernel;

    AddService(new AM_APP_Interface);
    AddService(new AM_NET_Interface);
    AddService(new AM_SYS_Interface);
    AddService(new AM_U_Interface);
}

void Shutdown() {}

} // namespace AM

} // namespace Service
