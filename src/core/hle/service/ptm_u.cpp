// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/ptm_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace PTM_U

namespace PTM_U {

static bool shell_open = true;

/*
 * PTM_User::GetShellState service function.
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Whether the 3DS's physical shell casing is open (1) or closed (0)
 */
static void GetShellState(Service::Interface* self) {
    u32* cmd_buff = Service::GetCommandBuffer();

    cmd_buff[1] = 0;
    cmd_buff[2] = shell_open ? 1 : 0;

    DEBUG_LOG(KERNEL, "PTM_U::GetShellState called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010002, nullptr,               "RegisterAlarmClient"},
    {0x00020080, nullptr,               "SetRtcAlarm"},
    {0x00030000, nullptr,               "GetRtcAlarm"},
    {0x00040000, nullptr,               "CancelRtcAlarm"},
    {0x00050000, nullptr,               "GetAdapterState"},
    {0x00060000, GetShellState,         "GetShellState"},
    {0x00070000, nullptr,               "GetBatteryLevel"},
    {0x00080000, nullptr,               "GetBatteryChargeState"},
    {0x00090000, nullptr,               "GetPedometerState"},
    {0x000A0042, nullptr,               "GetStepHistoryEntry"},
    {0x000B00C2, nullptr,               "GetStepHistory"},
    {0x000C0000, nullptr,               "GetTotalStepCount"},
    {0x000D0040, nullptr,               "SetPedometerRecordingMode"},
    {0x000E0000, nullptr,               "GetPedometerRecordingMode"},
    {0x000F0084, nullptr,               "GetStepHistoryAll"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
