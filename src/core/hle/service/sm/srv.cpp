// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/semaphore.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/service/sm/sm.h"
#include "core/hle/service/sm/srv.h"

namespace Service {
namespace SM {

constexpr int MAX_PENDING_NOTIFICATIONS = 16;

/**
 * SRV::RegisterClient service function
 *  Inputs:
 *      0: 0x00010002
 *      1: ProcessId Header (must be 0x20)
 *  Outputs:
 *      0: 0x00010040
 *      1: ResultCode
 */
void SRV::RegisterClient(Kernel::HLERequestContext& ctx) {
    u32* cmd_buff = ctx.CommandBuffer();

    if (cmd_buff[1] != IPC::CallingPidDesc()) {
        cmd_buff[0] = IPC::MakeHeader(0x0, 0x1, 0); // 0x40
        cmd_buff[1] = IPC::ERR_INVALID_BUFFER_DESCRIPTOR.raw;
        return;
    }
    cmd_buff[0] = IPC::MakeHeader(0x1, 0x1, 0); // 0x10040
    cmd_buff[1] = RESULT_SUCCESS.raw;           // No error
    LOG_WARNING(Service_SRV, "(STUBBED) called");
}

/**
 * SRV::EnableNotification service function
 *  Inputs:
 *      0: 0x00020000
 *  Outputs:
 *      0: 0x00020042
 *      1: ResultCode
 *      2: Translation descriptor: 0x20
 *      3: Handle to semaphore signaled on process notification
 */
void SRV::EnableNotification(Kernel::HLERequestContext& ctx) {
    u32* cmd_buff = ctx.CommandBuffer();

    notification_semaphore =
        Kernel::Semaphore::Create(0, MAX_PENDING_NOTIFICATIONS, "SRV:Notification").Unwrap();

    cmd_buff[0] = IPC::MakeHeader(0x2, 0x1, 0x2); // 0x20042
    cmd_buff[1] = RESULT_SUCCESS.raw;             // No error
    cmd_buff[2] = IPC::CopyHandleDesc(1);
    cmd_buff[3] = Kernel::g_handle_table.Create(notification_semaphore).MoveFrom();
    LOG_WARNING(Service_SRV, "(STUBBED) called");
}

/**
 * SRV::GetServiceHandle service function
 *  Inputs:
 *      0: 0x00050100
 *      1-2: 8-byte UTF-8 service name
 *      3: Name length
 *      4: Flags (bit0: if not set, return port-handle if session-handle unavailable)
 *  Outputs:
 *      1: ResultCode
 *      3: Service handle
 */
void SRV::GetServiceHandle(Kernel::HLERequestContext& ctx) {
    ResultCode res = RESULT_SUCCESS;
    u32* cmd_buff = ctx.CommandBuffer();

    size_t name_len = cmd_buff[3];
    if (name_len > Service::kMaxPortSize) {
        cmd_buff[1] = ERR_INVALID_NAME_SIZE.raw;
        LOG_ERROR(Service_SRV, "called name_len=0x%X, failed with code=0x%08X", name_len,
                  cmd_buff[1]);
        return;
    }
    std::string name(reinterpret_cast<const char*>(&cmd_buff[1]), name_len);
    bool return_port_on_failure = (cmd_buff[4] & 1) == 0;

    // TODO(yuriks): Permission checks go here

    auto client_port = service_manager->GetServicePort(name);
    if (client_port.Failed()) {
        cmd_buff[1] = client_port.Code().raw;
        LOG_ERROR(Service_SRV, "called service=%s, failed with code=0x%08X", name.c_str(),
                  cmd_buff[1]);
        return;
    }

    auto session = client_port.Unwrap()->Connect();
    cmd_buff[1] = session.Code().raw;
    if (session.Succeeded()) {
        cmd_buff[3] = Kernel::g_handle_table.Create(session.MoveFrom()).MoveFrom();
        LOG_DEBUG(Service_SRV, "called service=%s, session handle=0x%08X", name.c_str(),
                  cmd_buff[3]);
    } else if (session.Code() == Kernel::ERR_MAX_CONNECTIONS_REACHED && return_port_on_failure) {
        cmd_buff[1] = ERR_MAX_CONNECTIONS_REACHED.raw;
        cmd_buff[3] = Kernel::g_handle_table.Create(client_port.MoveFrom()).MoveFrom();
        LOG_WARNING(Service_SRV, "called service=%s, *port* handle=0x%08X", name.c_str(),
                    cmd_buff[3]);
    } else {
        LOG_ERROR(Service_SRV, "called service=%s, failed with code=0x%08X", name.c_str(),
                  cmd_buff[1]);
    }
}

/**
 * SRV::Subscribe service function
 *  Inputs:
 *      0: 0x00090040
 *      1: Notification ID
 *  Outputs:
 *      0: 0x00090040
 *      1: ResultCode
 */
void SRV::Subscribe(Kernel::HLERequestContext& ctx) {
    u32* cmd_buff = ctx.CommandBuffer();

    u32 notification_id = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0x9, 0x1, 0); // 0x90040
    cmd_buff[1] = RESULT_SUCCESS.raw;           // No error
    LOG_WARNING(Service_SRV, "(STUBBED) called, notification_id=0x%X", notification_id);
}

/**
 * SRV::Unsubscribe service function
 *  Inputs:
 *      0: 0x000A0040
 *      1: Notification ID
 *  Outputs:
 *      0: 0x000A0040
 *      1: ResultCode
 */
void SRV::Unsubscribe(Kernel::HLERequestContext& ctx) {
    u32* cmd_buff = ctx.CommandBuffer();

    u32 notification_id = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0xA, 0x1, 0); // 0xA0040
    cmd_buff[1] = RESULT_SUCCESS.raw;           // No error
    LOG_WARNING(Service_SRV, "(STUBBED) called, notification_id=0x%X", notification_id);
}

/**
 * SRV::PublishToSubscriber service function
 *  Inputs:
 *      0: 0x000C0080
 *      1: Notification ID
 *      2: Flags (bit0: only fire if not fired, bit1: report errors)
 *  Outputs:
 *      0: 0x000C0040
 *      1: ResultCode
 */
void SRV::PublishToSubscriber(Kernel::HLERequestContext& ctx) {
    u32* cmd_buff = ctx.CommandBuffer();

    u32 notification_id = cmd_buff[1];
    u8 flags = cmd_buff[2] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0xC, 0x1, 0); // 0xC0040
    cmd_buff[1] = RESULT_SUCCESS.raw;           // No error
    LOG_WARNING(Service_SRV, "(STUBBED) called, notification_id=0x%X, flags=%u", notification_id,
                flags);
}

SRV::SRV(std::shared_ptr<ServiceManager> service_manager)
    : ServiceFramework("srv:", 4), service_manager(std::move(service_manager)) {
    static const FunctionInfo functions[] = {
        {0x00010002, &SRV::RegisterClient, "RegisterClient"},
        {0x00020000, &SRV::EnableNotification, "EnableNotification"},
        {0x00030100, nullptr, "RegisterService"},
        {0x000400C0, nullptr, "UnregisterService"},
        {0x00050100, &SRV::GetServiceHandle, "GetServiceHandle"},
        {0x000600C2, nullptr, "RegisterPort"},
        {0x000700C0, nullptr, "UnregisterPort"},
        {0x00080100, nullptr, "GetPort"},
        {0x00090040, &SRV::Subscribe, "Subscribe"},
        {0x000A0040, &SRV::Unsubscribe, "Unsubscribe"},
        {0x000B0000, nullptr, "ReceiveNotification"},
        {0x000C0080, &SRV::PublishToSubscriber, "PublishToSubscriber"},
        {0x000D0040, nullptr, "PublishAndGetSubscriber"},
        {0x000E00C0, nullptr, "IsServiceRegistered"},
    };
    RegisterHandlers(functions);
}

SRV::~SRV() = default;

} // namespace SM
} // namespace Service
