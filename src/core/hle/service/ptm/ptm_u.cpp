// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/ptm/ptm_u.h"

SERIALIZE_EXPORT_IMPL(Service::PTM::PTM_U)

namespace Service::PTM {

PTM_U::PTM_U(std::shared_ptr<Module> ptm) : Module::Interface(std::move(ptm), "ptm:u", 26) {
    static const FunctionInfo functions[] = {
        {0x00010002, nullptr, "RegisterAlarmClient"},
        {0x00020080, nullptr, "SetRtcAlarm"},
        {0x00030000, nullptr, "GetRtcAlarm"},
        {0x00040000, nullptr, "CancelRtcAlarm"},
        {0x00050000, &PTM_U::GetAdapterState, "GetAdapterState"},
        {0x00060000, &PTM_U::GetShellState, "GetShellState"},
        {0x00070000, &PTM_U::GetBatteryLevel, "GetBatteryLevel"},
        {0x00080000, &PTM_U::GetBatteryChargeState, "GetBatteryChargeState"},
        {0x00090000, &PTM_U::GetPedometerState, "GetPedometerState"},
        {0x000A0042, nullptr, "GetStepHistoryEntry"},
        {0x000B00C2, &PTM_U::GetStepHistory, "GetStepHistory"},
        {0x000C0000, &PTM_U::GetTotalStepCount, "GetTotalStepCount"},
        {0x000D0040, nullptr, "SetPedometerRecordingMode"},
        {0x000E0000, nullptr, "GetPedometerRecordingMode"},
        {0x000F0084, nullptr, "GetStepHistoryAll"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::PTM
