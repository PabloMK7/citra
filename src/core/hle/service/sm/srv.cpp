// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <tuple>
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/hle_ipc.h"
#include "core/hle/kernel/semaphore.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/lock.h"
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
    IPC::RequestParser rp(ctx, 0x1, 0, 2);

    u32 pid_descriptor = rp.Pop<u32>();
    if (pid_descriptor != IPC::CallingPidDesc()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(IPC::ERR_INVALID_BUFFER_DESCRIPTOR);
        return;
    }
    u32 caller_pid = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
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
    IPC::RequestParser rp(ctx, 0x2, 0, 0);

    notification_semaphore =
        Kernel::Semaphore::Create(0, MAX_PENDING_NOTIFICATIONS, "SRV:Notification").Unwrap();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyObjects(notification_semaphore);
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
    IPC::RequestParser rp(ctx, 0x5, 4, 0);
    auto name_buf = rp.PopRaw<std::array<char, 8>>();
    size_t name_len = rp.Pop<u32>();
    u32 flags = rp.Pop<u32>();

    bool wait_until_available = (flags & 1) == 0;

    if (name_len > Service::kMaxPortSize) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(ERR_INVALID_NAME_SIZE);
        LOG_ERROR(Service_SRV, "called name_len=0x{:X} -> ERR_INVALID_NAME_SIZE", name_len);
        return;
    }
    std::string name(name_buf.data(), name_len);

    // TODO(yuriks): Permission checks go here

    auto get_handle = [name, this](Kernel::SharedPtr<Kernel::Thread> thread,
                                   Kernel::HLERequestContext& ctx, ThreadWakeupReason reason) {
        LOG_ERROR(Service_SRV, "called service={} wakeup", name);
        auto client_port = service_manager->GetServicePort(name);

        auto session = client_port.Unwrap()->Connect();
        if (session.Succeeded()) {
            LOG_DEBUG(Service_SRV, "called service={} -> session={}", name,
                      (*session)->GetObjectId());
            IPC::RequestBuilder rb(ctx, 0x5, 1, 2);
            rb.Push(session.Code());
            rb.PushMoveObjects(std::move(session).Unwrap());
        } else if (session.Code() == Kernel::ERR_MAX_CONNECTIONS_REACHED) {
            LOG_ERROR(Service_SRV, "called service={} -> ERR_MAX_CONNECTIONS_REACHED", name);
            UNREACHABLE();
        } else {
            LOG_ERROR(Service_SRV, "called service={} -> error 0x{:08X}", name, session.Code().raw);
            IPC::RequestBuilder rb(ctx, 0x5, 1, 0);
            rb.Push(session.Code());
        }
    };

    auto client_port = service_manager->GetServicePort(name);
    if (client_port.Failed()) {
        if (wait_until_available && client_port.Code() == ERR_SERVICE_NOT_REGISTERED) {
            LOG_INFO(Service_SRV, "called service={} delayed", name);
            Kernel::SharedPtr<Kernel::Event> get_service_handle_event =
                ctx.SleepClientThread(Kernel::GetCurrentThread(), "GetServiceHandle",
                                      std::chrono::nanoseconds(-1), get_handle);
            get_service_handle_delayed_map[name] = std::move(get_service_handle_event);
            return;
        } else {
            IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
            rb.Push(client_port.Code());
            LOG_ERROR(Service_SRV, "called service={} -> error 0x{:08X}", name,
                      client_port.Code().raw);
            return;
        }
    }

    auto session = client_port.Unwrap()->Connect();
    if (session.Succeeded()) {
        LOG_DEBUG(Service_SRV, "called service={} -> session={}", name, (*session)->GetObjectId());
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
        rb.Push(session.Code());
        rb.PushMoveObjects(std::move(session).Unwrap());
    } else if (session.Code() == Kernel::ERR_MAX_CONNECTIONS_REACHED && wait_until_available) {
        LOG_WARNING(Service_SRV, "called service={} -> ERR_MAX_CONNECTIONS_REACHED", name);
        // TODO(Subv): Put the caller guest thread to sleep until this port becomes available again.
        UNIMPLEMENTED_MSG("Unimplemented wait until port {} is available.", name);
    } else {
        LOG_ERROR(Service_SRV, "called service={} -> error 0x{:08X}", name, session.Code().raw);
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(session.Code());
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
    IPC::RequestParser rp(ctx, 0x9, 1, 0);
    u32 notification_id = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_SRV, "(STUBBED) called, notification_id=0x{:X}", notification_id);
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
    IPC::RequestParser rp(ctx, 0xA, 1, 0);
    u32 notification_id = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_SRV, "(STUBBED) called, notification_id=0x{:X}", notification_id);
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
    IPC::RequestParser rp(ctx, 0xC, 2, 0);
    u32 notification_id = rp.Pop<u32>();
    u8 flags = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_SRV, "(STUBBED) called, notification_id=0x{:X}, flags={}", notification_id,
                flags);
}

void SRV::RegisterService(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x3, 4, 0);

    auto name_buf = rp.PopRaw<std::array<char, 8>>();
    size_t name_len = rp.Pop<u32>();
    u32 max_sessions = rp.Pop<u32>();

    std::string name(name_buf.data(), std::min(name_len, name_buf.size()));

    auto port = service_manager->RegisterService(name, max_sessions);

    if (port.Failed()) {
        IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
        rb.Push(port.Code());
        LOG_ERROR(Service_SRV, "called service={} -> error 0x{:08X}", name, port.Code().raw);
        return;
    }

    auto it = get_service_handle_delayed_map.find(name);
    if (it != get_service_handle_delayed_map.end()) {
        it->second->Signal();
        get_service_handle_delayed_map.erase(it);
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushMoveObjects(port.Unwrap());
}

SRV::SRV(std::shared_ptr<ServiceManager> service_manager)
    : ServiceFramework("srv:", 4), service_manager(std::move(service_manager)) {
    static const FunctionInfo functions[] = {
        {0x00010002, &SRV::RegisterClient, "RegisterClient"},
        {0x00020000, &SRV::EnableNotification, "EnableNotification"},
        {0x00030100, &SRV::RegisterService, "RegisterService"},
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
