// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "ndm_u.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace NDM_U

namespace NDM_U {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010042, nullptr,                 "EnterExclusiveState"},
    {0x00020002, nullptr,                 "LeaveExclusiveState"},
    {0x00030000, nullptr,                 "QueryExclusiveMode"},
    {0x00040002, nullptr,                 "LockState"},
    {0x00050002, nullptr,                 "UnlockState"},
    {0x00060040, nullptr,                 "SuspendDaemons"},
    {0x00070040, nullptr,                 "ResumeDaemons"},
    {0x00080040, nullptr,                 "DisableWifiUsage"},
    {0x00090000, nullptr,                 "EnableWifiUsage"},
    {0x000A0000, nullptr,                 "GetCurrentState"},
    {0x000B0000, nullptr,                 "GetTargetState"},
    {0x000C0000, nullptr,                 "<Stubbed>"},
    {0x000D0040, nullptr,                 "QueryStatus"},
    {0x000E0040, nullptr,                 "GetDaemonDisableCount"},
    {0x000F0000, nullptr,                 "GetSchedulerDisableCount"},
    {0x00100040, nullptr,                 "SetScanInterval"},
    {0x00110000, nullptr,                 "GetScanInterval"},
    {0x00120040, nullptr,                 "SetRetryInterval"},
    {0x00130000, nullptr,                 "GetRetryInterval"},
    {0x00140040, nullptr,                 "OverrideDefaultDaemons"},
    {0x00150000, nullptr,                 "ResetDefaultDaemons"},
    {0x00160000, nullptr,                 "GetDefaultDaemons"},
    {0x00170000, nullptr,                 "ClearHalfAwakeMacFilter"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable);
}

} // namespace
