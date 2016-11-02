// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/ac_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace AC_U

namespace AC_U {

struct ACConfig {
    std::array<u8, 0x200> data;
};

static ACConfig default_config{};

static bool ac_connected = false;

static Kernel::SharedPtr<Kernel::Event> close_event;
static Kernel::SharedPtr<Kernel::Event> connect_event;
static Kernel::SharedPtr<Kernel::Event> disconnect_event;

/**
 * AC_U::CreateDefaultConfig service function
 *  Inputs:
 *      64 : ACConfig size << 14 | 2
 *      65 : pointer to ACConfig struct
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void CreateDefaultConfig(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 ac_config_addr = cmd_buff[65];

    ASSERT_MSG(cmd_buff[64] == (sizeof(ACConfig) << 14 | 2),
               "Output buffer size not equal ACConfig size");

    Memory::WriteBlock(ac_config_addr, &default_config, sizeof(ACConfig));
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

/**
 * AC_U::ConnectAsync service function
 *  Inputs:
 *      1 : ProcessId Header
 *      3 : Copy Handle Header
 *      4 : Connection Event handle
 *      5 : ACConfig size << 14 | 2
 *      6 : pointer to ACConfig struct
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void ConnectAsync(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    connect_event = Kernel::g_handle_table.Get<Kernel::Event>(cmd_buff[4]);
    if (connect_event) {
        connect_event->name = "AC_U:connect_event";
        connect_event->Signal();
        ac_connected = true;
    }
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

/**
 * AC_U::GetConnectResult service function
 *  Inputs:
 *      1 : ProcessId Header
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void GetConnectResult(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

/**
 * AC_U::CloseAsync service function
 *  Inputs:
 *      1 : ProcessId Header
 *      3 : Copy Handle Header
 *      4 : Event handle, should be signaled when AC connection is closed
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void CloseAsync(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    if (ac_connected && disconnect_event) {
        disconnect_event->Signal();
    }

    close_event = Kernel::g_handle_table.Get<Kernel::Event>(cmd_buff[4]);
    if (close_event) {
        close_event->name = "AC_U:close_event";
        close_event->Signal();
    }

    ac_connected = false;

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_AC, "(STUBBED) called");
}

/**
 * AC_U::GetCloseResult service function
 *  Inputs:
 *      1 : ProcessId Header
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void GetCloseResult(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

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
 * AC_U::GetInfraPriority service function
 *  Inputs:
 *      1 : ACConfig size << 14 | 2
 *      2 : pointer to ACConfig struct
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Infra Priority
 */
static void GetInfraPriority(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0;                  // Infra Priority, default 0

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

/**
 * AC_U::SetRequestEulaVersion service function
 *  Inputs:
 *      1 : Eula Version major
 *      2 : Eula Version minor
 *      3 : ACConfig size << 14 | 2
 *      4 : Input pointer to ACConfig struct
 *      64 : ACConfig size << 14 | 2
 *      65 : Output pointer to ACConfig struct
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Infra Priority
 */
static void SetRequestEulaVersion(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 major = cmd_buff[1] & 0xFF;
    u32 minor = cmd_buff[2] & 0xFF;

    ASSERT_MSG(cmd_buff[3] == (sizeof(ACConfig) << 14 | 2),
               "Input buffer size not equal ACConfig size");
    ASSERT_MSG(cmd_buff[64] == (sizeof(ACConfig) << 14 | 2),
               "Output buffer size not equal ACConfig size");

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0;                  // Infra Priority

    LOG_WARNING(Service_AC, "(STUBBED) called, major=%u, minor=%u", major, minor);
}

/**
 * AC_U::RegisterDisconnectEvent service function
 *  Inputs:
 *      1 : ProcessId Header
 *      3 : Copy Handle Header
 *      4 : Event handle, should be signaled when AC connection is closed
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void RegisterDisconnectEvent(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    disconnect_event = Kernel::g_handle_table.Get<Kernel::Event>(cmd_buff[4]);
    if (disconnect_event) {
        disconnect_event->name = "AC_U:disconnect_event";
    }
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

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
    cmd_buff[2] = ac_connected;

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010000, CreateDefaultConfig, "CreateDefaultConfig"},
    {0x00040006, ConnectAsync, "ConnectAsync"},
    {0x00050002, GetConnectResult, "GetConnectResult"},
    {0x00070002, nullptr, "CancelConnectAsync"},
    {0x00080004, CloseAsync, "CloseAsync"},
    {0x00090002, GetCloseResult, "GetCloseResult"},
    {0x000A0000, nullptr, "GetLastErrorCode"},
    {0x000C0000, nullptr, "GetStatus"},
    {0x000D0000, GetWifiStatus, "GetWifiStatus"},
    {0x000E0042, nullptr, "GetCurrentAPInfo"},
    {0x00100042, nullptr, "GetCurrentNZoneInfo"},
    {0x00110042, nullptr, "GetNZoneApNumService"},
    {0x001D0042, nullptr, "ScanAPs"},
    {0x00240042, nullptr, "AddDenyApType"},
    {0x00270002, GetInfraPriority, "GetInfraPriority"},
    {0x002D0082, SetRequestEulaVersion, "SetRequestEulaVersion"},
    {0x00300004, RegisterDisconnectEvent, "RegisterDisconnectEvent"},
    {0x003C0042, nullptr, "GetAPSSIDList"},
    {0x003E0042, IsConnected, "IsConnected"},
    {0x00400042, nullptr, "SetClientVersion"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);

    ac_connected = false;

    close_event = nullptr;
    connect_event = nullptr;
    disconnect_event = nullptr;
}

Interface::~Interface() {
    close_event = nullptr;
    connect_event = nullptr;
    disconnect_event = nullptr;
}

} // namespace
