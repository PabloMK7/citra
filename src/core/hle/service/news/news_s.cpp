// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/ipc_helpers.h"
#include "core/hle/service/news/news_s.h"

namespace Service::NEWS {

void NEWS_S::GetTotalNotifications(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x5, 0, 0);

    LOG_WARNING(Service, "(STUBBED) called");

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);
}

NEWS_S::NEWS_S() : ServiceFramework("news:s", 2) {
    const FunctionInfo functions[] = {
        {0x000100C6, nullptr, "AddNotification"},
        {0x00050000, &NEWS_S::GetTotalNotifications, "GetTotalNotifications"},
        {0x00060042, nullptr, "SetNewsDBHeader"},
        {0x00070082, nullptr, "SetNotificationHeader"},
        {0x00080082, nullptr, "SetNotificationMessage"},
        {0x00090082, nullptr, "SetNotificationImage"},
        {0x000A0042, nullptr, "GetNewsDBHeader"},
        {0x000B0082, nullptr, "GetNotificationHeader"},
        {0x000C0082, nullptr, "GetNotificationMessage"},
        {0x000D0082, nullptr, "GetNotificationImage"},
        {0x000E0040, nullptr, "SetInfoLEDPattern"},
        {0x00120082, nullptr, "GetNotificationHeaderOther"},
        {0x00130000, nullptr, "WriteNewsDBSavedata"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::NEWS
