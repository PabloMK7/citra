// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/ipc_helpers.h"
#include "core/hle/service/news/news.h"
#include "core/hle/service/news/news_s.h"

namespace Service {
namespace NEWS {

/**
 * GetTotalNotifications service function.
 *  Inputs:
 *      0 : 0x00050000
 *  Outputs:
 *      0 : 0x00050080
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Number of notifications
 */
static void GetTotalNotifications(Service::Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x5, 0, 0);

    LOG_WARNING(Service, "(STUBBED) called");

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x000100C6, nullptr, "AddNotification"},
    {0x00050000, GetTotalNotifications, "GetTotalNotifications"},
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

NEWS_S_Interface::NEWS_S_Interface() {
    Register(FunctionTable);
}

} // namespace NEWS
} // namespace Service
