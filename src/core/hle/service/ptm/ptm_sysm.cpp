// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/ptm/ptm_sysm.h"

SERIALIZE_EXPORT_IMPL(Service::PTM::PTM_S)
SERIALIZE_EXPORT_IMPL(Service::PTM::PTM_Sysm)

namespace Service::PTM {

PTM_S_Common::PTM_S_Common(std::shared_ptr<Module> ptm, const char* name)
    : Module::Interface(std::move(ptm), name, 26) {
    static const FunctionInfo functions[] = {
        // ptm:u common commands
        // clang-format off
        {IPC::MakeHeader(0x0001, 0, 2), nullptr, "RegisterAlarmClient"},
        {IPC::MakeHeader(0x0002, 2, 0), nullptr, "SetRtcAlarm"},
        {IPC::MakeHeader(0x0003, 0, 0), nullptr, "GetRtcAlarm"},
        {IPC::MakeHeader(0x0004, 0, 0), nullptr, "CancelRtcAlarm"},
        {IPC::MakeHeader(0x0005, 0, 0), &PTM_S_Common::GetAdapterState, "GetAdapterState"},
        {IPC::MakeHeader(0x0006, 0, 0), &PTM_S_Common::GetShellState, "GetShellState"},
        {IPC::MakeHeader(0x0007, 0, 0), &PTM_S_Common::GetBatteryLevel, "GetBatteryLevel"},
        {IPC::MakeHeader(0x0008, 0, 0), &PTM_S_Common::GetBatteryChargeState, "GetBatteryChargeState"},
        {IPC::MakeHeader(0x0009, 0, 0), nullptr, "GetPedometerState"},
        {IPC::MakeHeader(0x000A, 1, 2), nullptr, "GetStepHistoryEntry"},
        {IPC::MakeHeader(0x000B, 3, 2), &PTM_S_Common::GetStepHistory, "GetStepHistory"},
        {IPC::MakeHeader(0x000C, 0, 0), &PTM_S_Common::GetTotalStepCount, "GetTotalStepCount"},
        {IPC::MakeHeader(0x000D, 1, 0), nullptr, "SetPedometerRecordingMode"},
        {IPC::MakeHeader(0x000E, 0, 0), nullptr, "GetPedometerRecordingMode"},
        {IPC::MakeHeader(0x000F, 2, 4), nullptr, "GetStepHistoryAll"},
        // ptm:sysm & ptm:s
        {IPC::MakeHeader(0x0401, 3, 0), nullptr, "SetRtcAlarmEx"},
        {IPC::MakeHeader(0x0402, 1, 2), nullptr, "ReplySleepQuery"},
        {IPC::MakeHeader(0x0403, 1, 2), nullptr, "NotifySleepPreparationComplete"},
        {IPC::MakeHeader(0x0404, 4, 2), nullptr, "SetWakeupTrigger"},
        {IPC::MakeHeader(0x0405, 0, 0), nullptr, "GetAwakeReason"},
        {IPC::MakeHeader(0x0406, 0, 0), nullptr, "RequestSleep"},
        {IPC::MakeHeader(0x0407, 3, 0), nullptr, "ShutdownAsync"},
        {IPC::MakeHeader(0x0408, 0, 0), nullptr, "Awake"},
        {IPC::MakeHeader(0x0409, 2, 0), nullptr, "RebootAsync"},
        {IPC::MakeHeader(0x040A, 0, 0), &PTM_S_Common::CheckNew3DS, "CheckNew3DS"},
        {IPC::MakeHeader(0x0801, 25, 0), nullptr, "SetInfoLEDPattern"},
        {IPC::MakeHeader(0x0802, 1, 0), nullptr, "SetInfoLEDPatternHeader"},
        {IPC::MakeHeader(0x0803, 0, 0), nullptr, "GetInfoLEDStatus"},
        {IPC::MakeHeader(0x0804, 1, 0), nullptr, "SetBatteryEmptyLEDPattern"},
        {IPC::MakeHeader(0x0805, 0, 0), nullptr, "ClearStepHistory"},
        {IPC::MakeHeader(0x0806, 3, 2), nullptr, "SetStepHistory"},
        {IPC::MakeHeader(0x0807, 2, 2), nullptr, "GetPlayHistory"},
        {IPC::MakeHeader(0x0808, 0, 0), nullptr, "GetPlayHistoryStart"},
        {IPC::MakeHeader(0x0809, 0, 0), nullptr, "GetPlayHistoryLength"},
        {IPC::MakeHeader(0x080A, 0, 0), nullptr, "ClearPlayHistory"},
        {IPC::MakeHeader(0x080B, 2, 0), nullptr, "CalcPlayHistoryStart"},
        {IPC::MakeHeader(0x080C, 2, 0), nullptr, "SetUserTime"},
        {IPC::MakeHeader(0x080D, 0, 0), nullptr, "InvalidateSystemTime"},
        {IPC::MakeHeader(0x080E, 5, 0), nullptr, "NotifyPlayEvent"},
        {IPC::MakeHeader(0x080F, 0, 0), &PTM_S_Common::GetSoftwareClosedFlag, "GetSoftwareClosedFlag"},
        {IPC::MakeHeader(0x0810, 0, 0), nullptr, "ClearSoftwareClosedFlag"},
        {IPC::MakeHeader(0x0811, 0, 0), &PTM_S_Common::GetShellState, "GetShellState"},
        {IPC::MakeHeader(0x0812, 0, 0), nullptr, "IsShutdownByBatteryEmpty"},
        {IPC::MakeHeader(0x0813, 0, 0), nullptr, "FormatSavedata"},
        {IPC::MakeHeader(0x0814, 0, 0), nullptr, "GetLegacyJumpProhibitedFlag"},
        {IPC::MakeHeader(0x0818, 1, 0), nullptr, "ConfigureNew3DSCPU"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

PTM_S::PTM_S(std::shared_ptr<Module> ptm) : PTM_S_Common(std::move(ptm), "ptm:s") {}

PTM_Sysm::PTM_Sysm(std::shared_ptr<Module> ptm) : PTM_S_Common(std::move(ptm), "ptm:sysm") {}

} // namespace Service::PTM
