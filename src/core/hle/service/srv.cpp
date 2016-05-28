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

static const char* NotifDesc(u32 notif_id) {
    switch (notif_id) {
    case 0x100: return "This indicates that all processes must terminate : power - off, reboot, or FIRM - launch.";
    case 0x104: return "This indicates that the system is entering sleep mode. (PTM : NotifySleepPreparationComplete needed for this and the following ? )";
    case 0x105: return "This indicates that the system has exited sleep mode.";
    case 0x107: return "Unknown.Subscribed to by CECD module.";
    case 0x108: return "error at boot ?";
    case 0x109: return "?(Subscribed to by GSP)";
    case 0x10C: return "Unknown.";
    //case 0x110 - 0x11F Unknown.See PM launch flags.
    case 0x179: return "Unknown";
    case 0x202: return "POWER button pressed";
    case 0x203: return "POWER button held long";
    case 0x204: return "HOME button pressed";
    case 0x205: return "HOME button released";
    case 0x206: return "This is signaled by NWMEXT : ControlWirelessEnabled and when the physical Wi - Fi slider is enabled";
    case 0x207: return "SD card inserted";
    case 0x208: return "Game cartridge inserted";
    case 0x209: return "SD card removed";
    case 0x20A: return "Game cartridge removed";
    case 0x20B: return "Game cartridge inserted or removed";
    case 0x20D: return "? (Subscribed to by GSP)";
    case 0x20E: return "? (Subscribed to by GSP)";
    case 0x213: return "? (Subscribed to by GSP)";
    case 0x214: return "? (Subscribed to by GSP)";
    case 0x302: return "Unknown.Signaled by nwm module.";
    case 0x303: return "Unknown.Subscribed to by CECD module.";
    case 0x304: return "Unknown.Subscribed to by CECD module";
    }
    return "Unknown notification";
}

/**
 * SRV::RegisterClient service function
 *  Inputs:
 *      0: 0x00010002
 *      1: ProcessId Header (must be 0x20)
 *  Outputs:
 *      1: ResultCode
 */
static void RegisterClient(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_SRV, "(STUBBED) called");
}

/**
 * SRV::EnableNotification service function
 *  Inputs:
 *      0: 0x00020000
 *  Outputs:
 *      1: ResultCode
 *      2: Translation descriptor: 0x20
 *      3: Handle to semaphore signaled on process notification
 */
static void EnableNotification(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(bunnei): Change to a semaphore once these have been implemented
    event_handle = Kernel::Event::Create(Kernel::ResetType::OneShot, "SRV:Event");
    event_handle->Clear();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0x20;
    cmd_buff[3] = Kernel::g_handle_table.Create(event_handle).MoveFrom();
    LOG_WARNING(Service_SRV, "(STUBBED) called");
}

/**
 * SRV::EnableNotification service function
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
 *      1: ResultCode
 */
static void Subscribe(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 notif_id = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_SRV, "(STUBBED) called, notif_id=0x%X, desc: %s", notif_id, NotifDesc(notif_id));
}

/**
 * SRV::Unsubscribe service function
 *  Inputs:
 *      0: 0x000A0040
 *      1: Notification ID
 *  Outputs:
 *      1: ResultCode
 */
static void Unsubscribe(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 notif_id = cmd_buff[1];

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_SRV, "(STUBBED) called, notif_id=0x%X, desc: %s", notif_id, NotifDesc(notif_id));
}

/**
 * SRV::PublishToSubscriber service function
 *  Inputs:
 *      0: 0x000C0080
 *      1: Notification ID
 *      2: Flags (bit0: only fire if not fired, bit1: report errors)
 *  Outputs:
 *      1: ResultCode
 */
static void PublishToSubscriber(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 notif_id = cmd_buff[1];
    u8 flags = cmd_buff[2] & 0xFF;

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_SRV, "(STUBBED) called, notif_id=0x%X, flags=%u, desc: %s", notif_id, flags, NotifDesc(notif_id));
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
