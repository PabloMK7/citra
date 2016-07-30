// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/logging/log.h"

#include "core/hle/service/srv.h"
#include "core/hle/kernel/event.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SRV

namespace SRV {

static Kernel::SharedPtr<Kernel::Event> event_handle;

/**
 * SRV::RegisterClient service function
 *  Inputs:
 *      0: 0x00010002
 *      1: ProcessId Header (must be 0x20)
 *  Outputs:
 *      0: 0x00010040
 *      1: ResultCode
 */
static void RegisterClient(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    if (cmd_buff[1] != IPC::CallingPidDesc()) {
        cmd_buff[0] = IPC::MakeHeader(0x0, 0x1, 0); //0x40
        cmd_buff[1] = ResultCode(ErrorDescription::OS_InvalidBufferDescriptor, ErrorModule::OS,
                                 ErrorSummary::WrongArgument, ErrorLevel::Permanent).raw;
        return;
    }
    cmd_buff[0] = IPC::MakeHeader(0x1, 0x1, 0); //0x10040
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
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
static void EnableNotification(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(bunnei): Change to a semaphore once these have been implemented
    event_handle = Kernel::Event::Create(Kernel::ResetType::OneShot, "SRV:Event");
    event_handle->Clear();

    cmd_buff[0] = IPC::MakeHeader(0x2, 0x1, 0x2); // 0x20042
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = IPC::CopyHandleDesc(1);
    cmd_buff[3] = Kernel::g_handle_table.Create(event_handle).MoveFrom();
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
static void GetServiceHandle(Service::Interface* self) {
    ResultCode res = RESULT_SUCCESS;
    u32* cmd_buff = Kernel::GetCommandBuffer();

    std::string port_name = std::string((const char*)&cmd_buff[1], 0, Service::kMaxPortSize);
    auto it = Service::g_srv_services.find(port_name);

    if (it != Service::g_srv_services.end()) {
        cmd_buff[3] = Kernel::g_handle_table.Create(it->second).MoveFrom();
        LOG_TRACE(Service_SRV, "called port=%s, handle=0x%08X", port_name.c_str(), cmd_buff[3]);
    } else {
        LOG_ERROR(Service_SRV, "(UNIMPLEMENTED) called port=%s", port_name.c_str());
        res = UnimplementedFunction(ErrorModule::SRV);
    }
    cmd_buff[1] = res.raw;
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
static void Subscribe(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 notification_id = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0x9, 0x1, 0); // 0x90040
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
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
static void Unsubscribe(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 notification_id = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0xA, 0x1, 0); // 0xA0040
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
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
static void PublishToSubscriber(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 notification_id = cmd_buff[1];
    u8 flags = cmd_buff[2] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0xC, 0x1, 0); // 0xC0040
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_SRV, "(STUBBED) called, notification_id=0x%X, flags=%u", notification_id, flags);
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010002, RegisterClient,          "RegisterClient"},
    {0x00020000, EnableNotification,      "EnableNotification"},
    {0x00030100, nullptr,                 "RegisterService"},
    {0x000400C0, nullptr,                 "UnregisterService"},
    {0x00050100, GetServiceHandle,        "GetServiceHandle"},
    {0x000600C2, nullptr,                 "RegisterPort"},
    {0x000700C0, nullptr,                 "UnregisterPort"},
    {0x00080100, nullptr,                 "GetPort"},
    {0x00090040, Subscribe,               "Subscribe"},
    {0x000A0040, Unsubscribe,             "Unsubscribe"},
    {0x000B0000, nullptr,                 "ReceiveNotification"},
    {0x000C0080, PublishToSubscriber,     "PublishToSubscriber"},
    {0x000D0040, nullptr,                 "PublishAndGetSubscriber"},
    {0x000E00C0, nullptr,                 "IsServiceRegistered"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
    event_handle = nullptr;
}

Interface::~Interface() {
    event_handle = nullptr;
}

} // namespace SRV
