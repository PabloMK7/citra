// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/ndm/ndm.h"
#include "core/hle/service/ndm/ndm_u.h"

namespace Service {
namespace NDM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010042, EnterExclusiveState, "EnterExclusiveState"},
    {0x00020002, LeaveExclusiveState, "LeaveExclusiveState"},
    {0x00030000, QueryExclusiveMode, "QueryExclusiveMode"},
    {0x00040002, LockState, "LockState"},
    {0x00050002, UnlockState, "UnlockState"},
    {0x00060040, SuspendDaemons, "SuspendDaemons"},
    {0x00070040, ResumeDaemons, "ResumeDaemons"},
    {0x00080040, SuspendScheduler, "SuspendScheduler"},
    {0x00090000, ResumeScheduler, "ResumeScheduler"},
    {0x000A0000, nullptr, "GetCurrentState"},
    {0x000B0000, nullptr, "GetTargetState"},
    {0x000C0000, nullptr, "<Stubbed>"},
    {0x000D0040, QueryStatus, "QueryStatus"},
    {0x000E0040, GetDaemonDisableCount, "GetDaemonDisableCount"},
    {0x000F0000, GetSchedulerDisableCount, "GetSchedulerDisableCount"},
    {0x00100040, SetScanInterval, "SetScanInterval"},
    {0x00110000, GetScanInterval, "GetScanInterval"},
    {0x00120040, SetRetryInterval, "SetRetryInterval"},
    {0x00130000, GetRetryInterval, "GetRetryInterval"},
    {0x00140040, OverrideDefaultDaemons, "OverrideDefaultDaemons"},
    {0x00150000, ResetDefaultDaemons, "ResetDefaultDaemons"},
    {0x00160000, GetDefaultDaemons, "GetDefaultDaemons"},
    {0x00170000, ClearHalfAwakeMacFilter, "ClearHalfAwakeMacFilter"},
};

NDM_U_Interface::NDM_U_Interface() {
    Register(FunctionTable);
}

} // namespace NDM
} // namespace Service
