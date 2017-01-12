// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>

#include "common/logging/log.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/ac/ac.h"
#include "core/hle/service/ac/ac_i.h"
#include "core/hle/service/ac/ac_u.h"

namespace Service {
namespace AC {

struct ACConfig {
    std::array<u8, 0x200> data;
};

static ACConfig default_config{};

static bool ac_connected = false;

static Kernel::SharedPtr<Kernel::Event> close_event;
static Kernel::SharedPtr<Kernel::Event> connect_event;
static Kernel::SharedPtr<Kernel::Event> disconnect_event;

void CreateDefaultConfig(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 ac_config_addr = cmd_buff[65];

    ASSERT_MSG(cmd_buff[64] == (sizeof(ACConfig) << 14 | 2),
               "Output buffer size not equal ACConfig size");

    Memory::WriteBlock(ac_config_addr, &default_config, sizeof(ACConfig));
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

void ConnectAsync(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    connect_event = Kernel::g_handle_table.Get<Kernel::Event>(cmd_buff[4]);
    if (connect_event) {
        connect_event->name = "AC:connect_event";
        connect_event->Signal();
        ac_connected = true;
    }
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

void GetConnectResult(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

void CloseAsync(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    if (ac_connected && disconnect_event) {
        disconnect_event->Signal();
    }

    close_event = Kernel::g_handle_table.Get<Kernel::Event>(cmd_buff[4]);
    if (close_event) {
        close_event->name = "AC:close_event";
        close_event->Signal();
    }

    ac_connected = false;

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_AC, "(STUBBED) called");
}

void GetCloseResult(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

void GetWifiStatus(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(purpasmart96): This function is only a stub,
    // it returns a valid result without implementing full functionality.

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0;                  // Connection type set to none

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

void GetInfraPriority(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0;                  // Infra Priority, default 0

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

void SetRequestEulaVersion(Interface* self) {
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

void RegisterDisconnectEvent(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    disconnect_event = Kernel::g_handle_table.Get<Kernel::Event>(cmd_buff[4]);
    if (disconnect_event) {
        disconnect_event->name = "AC:disconnect_event";
    }
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

void IsConnected(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = ac_connected;

    LOG_WARNING(Service_AC, "(STUBBED) called");
}

void SetClientVersion(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const u32 version = cmd_buff[1];
    self->SetVersion(version);

    LOG_WARNING(Service_AC, "(STUBBED) called, version: 0x%08X", version);

    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
}

void Init() {
    AddService(new AC_I);
    AddService(new AC_U);

    ac_connected = false;

    close_event = nullptr;
    connect_event = nullptr;
    disconnect_event = nullptr;
}

void Shutdown() {
    ac_connected = false;

    close_event = nullptr;
    connect_event = nullptr;
    disconnect_event = nullptr;
}

} // namespace AC
} // namespace Service
