// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/ptm/ptm_sysm.h"

namespace Service::PTM {

PTM_S_Common::PTM_S_Common(std::shared_ptr<Module> ptm, const char* name)
    : Module::Interface(std::move(ptm), name, 26) {
    static const FunctionInfo functions[] = {
        // ptm:u common commands
        {0x00010002, nullptr, "RegisterAlarmClient"},
        {0x00020080, nullptr, "SetRtcAlarm"},
        {0x00030000, nullptr, "GetRtcAlarm"},
        {0x00040000, nullptr, "CancelRtcAlarm"},
        {0x00050000, &PTM_S_Common::GetAdapterState, "GetAdapterState"},
        {0x00060000, &PTM_S_Common::GetShellState, "GetShellState"},
        {0x00070000, &PTM_S_Common::GetBatteryLevel, "GetBatteryLevel"},
        {0x00080000, &PTM_S_Common::GetBatteryChargeState, "GetBatteryChargeState"},
        {0x00090000, nullptr, "GetPedometerState"},
        {0x000A0042, nullptr, "GetStepHistoryEntry"},
        {0x000B00C2, &PTM_S_Common::GetStepHistory, "GetStepHistory"},
        {0x000C0000, &PTM_S_Common::GetTotalStepCount, "GetTotalStepCount"},
        {0x000D0040, nullptr, "SetPedometerRecordingMode"},
        {0x000E0000, nullptr, "GetPedometerRecordingMode"},
        {0x000F0084, nullptr, "GetStepHistoryAll"},
        // ptm:sysm & ptm:s
        {0x040100C0, nullptr, "SetRtcAlarmEx"},
        {0x04020042, nullptr, "ReplySleepQuery"},
        {0x04030042, nullptr, "NotifySleepPreparationComplete"},
        {0x04040102, nullptr, "SetWakeupTrigger"},
        {0x04050000, nullptr, "GetAwakeReason"},
        {0x04060000, nullptr, "RequestSleep"},
        {0x040700C0, nullptr, "ShutdownAsync"},
        {0x04080000, nullptr, "Awake"},
        {0x04090080, nullptr, "RebootAsync"},
        {0x040A0000, &PTM_S_Common::CheckNew3DS, "CheckNew3DS"},
        {0x08010640, nullptr, "SetInfoLEDPattern"},
        {0x08020040, nullptr, "SetInfoLEDPatternHeader"},
        {0x08030000, nullptr, "GetInfoLEDStatus"},
        {0x08040040, nullptr, "SetBatteryEmptyLEDPattern"},
        {0x08050000, nullptr, "ClearStepHistory"},
        {0x080600C2, nullptr, "SetStepHistory"},
        {0x08070082, nullptr, "GetPlayHistory"},
        {0x08080000, nullptr, "GetPlayHistoryStart"},
        {0x08090000, nullptr, "GetPlayHistoryLength"},
        {0x080A0000, nullptr, "ClearPlayHistory"},
        {0x080B0080, nullptr, "CalcPlayHistoryStart"},
        {0x080C0080, nullptr, "SetUserTime"},
        {0x080D0000, nullptr, "InvalidateSystemTime"},
        {0x080E0140, nullptr, "NotifyPlayEvent"},
        {0x080F0000, &PTM_S_Common::GetSoftwareClosedFlag, "GetSoftwareClosedFlag"},
        {0x08100000, nullptr, "ClearSoftwareClosedFlag"},
        {0x08110000, &PTM_S_Common::GetShellState, "GetShellState"},
        {0x08120000, nullptr, "IsShutdownByBatteryEmpty"},
        {0x08130000, nullptr, "FormatSavedata"},
        {0x08140000, nullptr, "GetLegacyJumpProhibitedFlag"},
        {0x08180040, nullptr, "ConfigureNew3DSCPU"},
    };
    RegisterHandlers(functions);
}

PTM_S::PTM_S(std::shared_ptr<Module> ptm) : PTM_S_Common(std::move(ptm), "ptm:s") {}

PTM_Sysm::PTM_Sysm(std::shared_ptr<Module> ptm) : PTM_S_Common(std::move(ptm), "ptm:sysm") {}

} // namespace Service::PTM
