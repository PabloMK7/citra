// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/service/service.h"
#include "core/hle/service/am/am.h"
#include "core/hle/service/am/am_app.h"
#include "core/hle/service/am/am_net.h"
#include "core/hle/service/am/am_sys.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"

namespace Service {
namespace AM {

void TitleIDListGetTotal(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 media_type = cmd_buff[1] & 0xFF;

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;

    LOG_WARNING(Service_AM, "(STUBBED) media_type %u", media_type);
}

void GetTitleIDList(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 num_titles = cmd_buff[1];
    u32 media_type = cmd_buff[2] & 0xFF;
    u32 addr       = cmd_buff[4];

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;

    LOG_WARNING(Service_AM, "(STUBBED) Requested %u titles from media type %u. Address=0x%08X", num_titles, media_type, addr);
}

void GetNumContentInfos(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 1; // Number of content infos plus one

    LOG_WARNING(Service_AM, "(STUBBED) called");
}

void Init() {
    using namespace Kernel;

    AddService(new AM_APP_Interface);
    AddService(new AM_NET_Interface);
    AddService(new AM_SYS_Interface);
}

void Shutdown() {

}

} // namespace AM

} // namespace Service
