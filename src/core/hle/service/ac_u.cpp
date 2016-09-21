// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/ac_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace AC_U

namespace AC_U {

/**
 * AC_U::CloseAsync service function
 *  Inputs:
 *      1 : Always 0x20
 *      3 : Always 0
 *      4 : Event handle, should be signaled when AC connection is closed
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void CloseAsync(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    auto evt = Kernel::g_handle_table.Get<Kernel::Event>(cmd_buff[4]);

    if (evt) {
        evt->name = "AC_U:close_event";
        evt->Signal();
    }
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_AC, "(STUBBED) called");
}
/**
 * AC_U::GetWifiStatus service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Output connection type, 0 = none, 1 = Old3DS Internet, 2 = New3DS Internet.
 */
static void GetWifiStatus(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(purpasmart96): This function is only a stub,
    // it returns a valid result without implementing full functionality.

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0;                  // Connection type set to none

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

/**
 * AC_U::IsConnected service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : bool, is connected
 */
static void IsConnected(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = false;              // Not connected to ac:u service

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010000, nullptr, "CreateDefaultConfig"},
    {0x00040006, nullptr, "ConnectAsync"},
    {0x00050002, nullptr, "GetConnectResult"},
    {0x00080004, CloseAsync, "CloseAsync"},
    {0x00090002, nullptr, "GetCloseResult"},
    {0x000A0000, nullptr, "GetLastErrorCode"},
    {0x000D0000, GetWifiStatus, "GetWifiStatus"},
    {0x000E0042, nullptr, "GetCurrentAPInfo"},
    {0x00100042, nullptr, "GetCurrentNZoneInfo"},
    {0x00110042, nullptr, "GetNZoneApNumService"},
    {0x00240042, nullptr, "AddDenyApType"},
    {0x00270002, nullptr, "GetInfraPriority"},
    {0x002D0082, nullptr, "SetRequestEulaVersion"},
    {0x00300004, nullptr, "RegisterDisconnectEvent"},
    {0x003C0042, nullptr, "GetAPSSIDList"},
    {0x003E0042, IsConnected, "IsConnected"},
    {0x00400042, nullptr, "SetClientVersion"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
