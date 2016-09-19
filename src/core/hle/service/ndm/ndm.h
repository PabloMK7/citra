// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Service {

class Interface;

namespace NDM {

enum class Daemon : u32 {
    Cec = 0,
    Boss = 1,
    Nim = 2,
    Friend = 3,
};

enum class DaemonMask : u32 {
    None = 0,
    Cec = (1 << static_cast<u32>(Daemon::Cec)),
    Boss = (1 << static_cast<u32>(Daemon::Boss)),
    Nim = (1 << static_cast<u32>(Daemon::Nim)),
    Friend = (1 << static_cast<u32>(Daemon::Friend)),
    Default = Cec | Friend,
    All = Cec | Boss | Nim | Friend,
};

enum class DaemonStatus : u32 { Busy = 0, Idle = 1, Suspending = 2, Suspended = 3 };

enum class ExclusiveState : u32 {
    None = 0,
    Infrastructure = 1,
    LocalCommunications = 2,
    Streetpass = 3,
    StreetpassData = 4,
};

/**
 *  NDM::EnterExclusiveState service function
 *  Inputs:
 *      0 : Header code [0x00010042]
 *      1 : Exclusive State
 *      2 : 0x20
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void EnterExclusiveState(Service::Interface* self);

/**
 *  NDM::LeaveExclusiveState service function
 *  Inputs:
 *      0 : Header code [0x00020002]
 *      1 : 0x20
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void LeaveExclusiveState(Service::Interface* self);

/**
 *  NDM::QueryExclusiveMode service function
 *  Inputs:
 *      0 : Header code [0x00030000]
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Current Exclusive State
 */
void QueryExclusiveMode(Service::Interface* self);

/**
 *  NDM::LockState service function
 *  Inputs:
 *      0 : Header code [0x00040002]
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void LockState(Service::Interface* self);

/**
 *  NDM::UnlockState service function
 *  Inputs:
 *      0 : Header code [0x00050002]
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void UnlockState(Service::Interface* self);

/**
 *  NDM::SuspendDaemons service function
 *  Inputs:
 *      0 : Header code [0x00060040]
 *      1 : Daemon bit mask
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void SuspendDaemons(Service::Interface* self);

/**
 *  NDM::ResumeDaemons service function
 *  Inputs:
 *      0 : Header code [0x00070040]
 *      1 : Daemon bit mask
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void ResumeDaemons(Service::Interface* self);

/**
 *  NDM::SuspendScheduler service function
 *  Inputs:
 *      0 : Header code [0x00080040]
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void SuspendScheduler(Service::Interface* self);

/**
 *  NDM::ResumeScheduler service function
 *  Inputs:
 *      0 : Header code [0x00090000]
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void ResumeScheduler(Service::Interface* self);

/**
 *  NDM::QueryStatus service function
 *  Inputs:
 *      0 : Header code [0x000D0040]
 *      1 : Daemon
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Daemon status
 */
void QueryStatus(Service::Interface* self);

/**
 *  NDM::GetDaemonDisableCount service function
 *  Inputs:
 *      0 : Header code [0x000E0040]
 *      1 : Daemon
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Current process disable count
 *      3 : Total disable count
 */
void GetDaemonDisableCount(Service::Interface* self);

/**
 *  NDM::GetSchedulerDisableCount service function
 *  Inputs:
 *      0 : Header code [0x000F0000]
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Current process disable count
 *      3 : Total disable count
 */
void GetSchedulerDisableCount(Service::Interface* self);

/**
 *  NDM::SetScanInterval service function
 *  Inputs:
 *      0 : Header code [0x00100040]
 *      1 : Interval (default = 30)
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void SetScanInterval(Service::Interface* self);

/**
 *  NDM::GetScanInterval service function
 *  Inputs:
 *      0 : Header code [0x00110000]
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Interval (default = 30)
 */
void GetScanInterval(Service::Interface* self);

/**
 *  NDM::SetRetryInterval service function
 *  Inputs:
 *      0 : Header code [0x00120040]
 *      1 : Interval (default = 10)
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void SetRetryInterval(Service::Interface* self);

/**
 *  NDM::GetRetryInterval service function
 *  Inputs:
 *      0 : Header code [0x00130000]
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Interval (default = 10)
 */
void GetRetryInterval(Service::Interface* self);

/**
 *  NDM::OverrideDefaultDaemons service function
 *  Inputs:
 *      0 : Header code [0x00140040]
 *      1 : Daemon bit mask
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void OverrideDefaultDaemons(Service::Interface* self);

/**
 *  NDM::ResetDefaultDaemons service function
 *  Inputs:
 *      0 : Header code [0x00150000]
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void ResetDefaultDaemons(Service::Interface* self);

/**
 *  NDM::GetDefaultDaemons service function
 *  Inputs:
 *      0 : Header code [0x00160000]
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 *      2 : Daemon bit mask
 *  Note:
 *      Gets the current default daemon bit mask. The default value is (DAEMONMASK_CEC |
 * DAEMONMASK_FRIENDS)
 */
void GetDefaultDaemons(Service::Interface* self);

/**
 *  NDM::ClearHalfAwakeMacFilter service function
 *  Inputs:
 *      0 : Header code [0x00170000]
 *  Outputs:
 *      1 : Result, 0 on success, otherwise error code
 */
void ClearHalfAwakeMacFilter(Service::Interface* self);

/// Initialize NDM service
void Init();

/// Shutdown NDM service
void Shutdown();

} // namespace NDM
} // namespace Service
