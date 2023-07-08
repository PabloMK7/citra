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
    IPC::RequestParser rp(ctx, 0x5, 0, 0);

    LOG_WARNING(Service, "(STUBBED) called");

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);
}

void NEWS_S::GetNewsDBHeader(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0xA, 1, 2);
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
        {IPC::MakeHeader(0x0001, 3, 6), nullptr, "AddNotification"},
        {IPC::MakeHeader(0x0005, 0, 0), &NEWS_S::GetTotalNotifications, "GetTotalNotifications"},
        {IPC::MakeHeader(0x0006, 1, 2), nullptr, "SetNewsDBHeader"},
        {IPC::MakeHeader(0x0007, 2, 2), nullptr, "SetNotificationHeader"},
        {IPC::MakeHeader(0x0008, 2, 2), nullptr, "SetNotificationMessage"},
        {IPC::MakeHeader(0x0009, 2, 2), nullptr, "SetNotificationImage"},
        {IPC::MakeHeader(0x000A, 1, 2), &NEWS_S::GetNewsDBHeader, "GetNewsDBHeader"},
        {IPC::MakeHeader(0x000B, 2, 2), nullptr, "GetNotificationHeader"},
        {IPC::MakeHeader(0x000C, 2, 2), nullptr, "GetNotificationMessage"},
        {IPC::MakeHeader(0x000D, 2, 2), nullptr, "GetNotificationImage"},
        {IPC::MakeHeader(0x000E, 1, 0), nullptr, "SetInfoLEDPattern"},
        {IPC::MakeHeader(0x0012, 2, 2), nullptr, "GetNotificationHeaderOther"},
        {IPC::MakeHeader(0x0013, 0, 0), nullptr, "WriteNewsDBSavedata"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::NEWS
