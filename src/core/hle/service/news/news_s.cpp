// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/news/news_s.h"

SERIALIZE_EXPORT_IMPL(Service::NEWS::NEWS_S)

namespace Service::NEWS {

struct NewsDbHeader {
    u8 unknown_one;
    u8 flags;
    INSERT_PADDING_BYTES(0xE);
};
static_assert(sizeof(NewsDbHeader) == 0x10, "News DB Header structure size is wrong");

void NEWS_S::GetTotalNotifications(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);

    LOG_WARNING(Service, "(STUBBED) called");

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);
}

void NEWS_S::GetNewsDBHeader(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx);
    const auto size = rp.Pop<u32>();
    auto output_buffer = rp.PopMappedBuffer();

    LOG_WARNING(Service, "(STUBBED) called size={}", size);

    NewsDbHeader dummy = {.unknown_one = 1, .flags = 0};
    output_buffer.Write(&dummy, 0, std::min(sizeof(NewsDbHeader), static_cast<std::size_t>(size)));

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(size);
}

NEWS_S::NEWS_S() : ServiceFramework("news:s", 2) {
    const FunctionInfo functions[] = {
        // clang-format off
        {0x0001, nullptr, "AddNotification"},
        {0x0005, &NEWS_S::GetTotalNotifications, "GetTotalNotifications"},
        {0x0006, nullptr, "SetNewsDBHeader"},
        {0x0007, nullptr, "SetNotificationHeader"},
        {0x0008, nullptr, "SetNotificationMessage"},
        {0x0009, nullptr, "SetNotificationImage"},
        {0x000A, &NEWS_S::GetNewsDBHeader, "GetNewsDBHeader"},
        {0x000B, nullptr, "GetNotificationHeader"},
        {0x000C, nullptr, "GetNotificationMessage"},
        {0x000D, nullptr, "GetNotificationImage"},
        {0x000E, nullptr, "SetInfoLEDPattern"},
        {0x0012, nullptr, "GetNotificationHeaderOther"},
        {0x0013, nullptr, "WriteNewsDBSavedata"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NEWS
