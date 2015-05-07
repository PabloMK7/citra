// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/hle.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/ptm/ptm_u.h"

namespace Service {
namespace PTM {

/**
 * PTM_U::GetAdapterState service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Output of function, 0 = not charging, 1 = charging.
 */
static void GetAdapterState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = GetAdapterState();

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

/*
 * PTM_User::GetShellState service function.
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Whether the 3DS's physical shell casing is open (1) or closed (0)
 */
static void GetShellState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = GetShellState();
}

/**
 * PTM_U::GetBatteryLevel service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Battery level, 5 = completely full battery, 4 = mostly full battery,
 *          3 = half full battery, 2 =  low battery, 1 = critical battery.
 */
static void GetBatteryLevel(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = static_cast<u32>(GetBatteryLevel());

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

/**
 * PTM_U::GetBatteryChargeState service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Output of function, 0 = not charging, 1 = charging.
 */
static void GetBatteryChargeState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // TODO(purpasmart96): This function is only a stub,
    // it returns a valid result without implementing full functionality.

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = GetAdapterState();

    LOG_WARNING(Service_PTM, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010002, nullptr,               "RegisterAlarmClient"},
    {0x00020080, nullptr,               "SetRtcAlarm"},
    {0x00030000, nullptr,               "GetRtcAlarm"},
    {0x00040000, nullptr,               "CancelRtcAlarm"},
    {0x00050000, GetAdapterState,       "GetAdapterState"},
    {0x00060000, GetShellState,         "GetShellState"},
    {0x00070000, GetBatteryLevel,       "GetBatteryLevel"},
    {0x00080000, GetBatteryChargeState, "GetBatteryChargeState"},
    {0x00090000, nullptr,               "GetPedometerState"},
    {0x000A0042, nullptr,               "GetStepHistoryEntry"},
    {0x000B00C2, nullptr,               "GetStepHistory"},
    {0x000C0000, nullptr,               "GetTotalStepCount"},
    {0x000D0040, nullptr,               "SetPedometerRecordingMode"},
    {0x000E0000, nullptr,               "GetPedometerRecordingMode"},
    {0x000F0084, nullptr,               "GetStepHistoryAll"},
};

PTM_U_Interface::PTM_U_Interface() {
    Register(FunctionTable);
}

} // namespace PTM
} // namespace Service
