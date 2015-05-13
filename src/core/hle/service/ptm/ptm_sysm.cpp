// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/ptm/ptm.h"
#include "core/hle/service/ptm/ptm_sysm.h"

namespace Service {
namespace PTM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x040100C0, nullptr,             "SetRtcAlarmEx"},
    {0x04020042, nullptr,             "ReplySleepQuery"},
    {0x04030042, nullptr,             "NotifySleepPreparationComplete"},
    {0x04040102, nullptr,             "SetWakeupTrigger"},
    {0x04050000, nullptr,             "GetAwakeReason"},
    {0x04060000, nullptr,             "RequestSleep"},
    {0x040700C0, nullptr,             "ShutdownAsync"},
    {0x04080000, nullptr,             "Awake"},
    {0x04090080, nullptr,             "RebootAsync"},
    {0x040A0000, nullptr,             "CheckNew3DS"},
    {0x08010640, nullptr,             "SetInfoLEDPattern"},
    {0x08020040, nullptr,             "SetInfoLEDPatternHeader"},
    {0x08030000, nullptr,             "GetInfoLEDStatus"},
    {0x08040040, nullptr,             "SetBatteryEmptyLEDPattern"},
    {0x08050000, nullptr,             "ClearStepHistory"},
    {0x080600C2, nullptr,             "SetStepHistory"},
    {0x08070082, nullptr,             "GetPlayHistory"},
    {0x08080000, nullptr,             "GetPlayHistoryStart"},
    {0x08090000, nullptr,             "GetPlayHistoryLength"},
    {0x080A0000, nullptr,             "ClearPlayHistory"},
    {0x080B0080, nullptr,             "CalcPlayHistoryStart"},
    {0x080C0080, nullptr,             "SetUserTime"},
    {0x080D0000, nullptr,             "InvalidateSystemTime"},
    {0x080E0140, nullptr,             "NotifyPlayEvent"},
    {0x080F0000, IsLegacyPowerOff,    "IsLegacyPowerOff"},
    {0x08100000, nullptr,             "ClearLegacyPowerOff"},
    {0x08110000, nullptr,             "GetShellStatus"},
    {0x08120000, nullptr,             "IsShutdownByBatteryEmpty"},
    {0x08130000, nullptr,             "FormatSavedata"},
    {0x08140000, nullptr,             "GetLegacyJumpProhibitedFlag"}
};

PTM_Sysm_Interface::PTM_Sysm_Interface() {
    Register(FunctionTable);
}

} // namespace PTM
} // namespace Service
