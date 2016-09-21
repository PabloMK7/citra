// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/service/ndm/ndm.h"
#include "core/hle/service/ndm/ndm_u.h"
#include "core/hle/service/service.h"

namespace Service {
namespace NDM {

enum : u32 {
    DEFAULT_RETRY_INTERVAL = 10,
    DEFAULT_SCAN_INTERVAL = 30,
};

static DaemonMask daemon_bit_mask = DaemonMask::Default;
static DaemonMask default_daemon_bit_mask = DaemonMask::Default;
static std::array<DaemonStatus, 4> daemon_status = {
    DaemonStatus::Idle, DaemonStatus::Idle, DaemonStatus::Idle, DaemonStatus::Idle,
};
static ExclusiveState exclusive_state = ExclusiveState::None;
static u32 scan_interval = DEFAULT_SCAN_INTERVAL;
static u32 retry_interval = DEFAULT_RETRY_INTERVAL;
static bool daemon_lock_enabled = false;

void EnterExclusiveState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    exclusive_state = static_cast<ExclusiveState>(cmd_buff[1]);

    cmd_buff[0] = IPC::MakeHeader(0x1, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NDM, "(STUBBED) exclusive_state=0x%08X ", exclusive_state);
}

void LeaveExclusiveState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    exclusive_state = ExclusiveState::None;

    cmd_buff[0] = IPC::MakeHeader(0x2, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NDM, "(STUBBED) exclusive_state=0x%08X ", exclusive_state);
}

void QueryExclusiveMode(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x3, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = static_cast<u32>(exclusive_state);
    LOG_WARNING(Service_NDM, "(STUBBED) exclusive_state=0x%08X ", exclusive_state);
}

void LockState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    daemon_lock_enabled = true;

    cmd_buff[0] = IPC::MakeHeader(0x4, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NDM, "(STUBBED) daemon_lock_enabled=0x%08X ", daemon_lock_enabled);
}

void UnlockState(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    daemon_lock_enabled = false;

    cmd_buff[0] = IPC::MakeHeader(0x5, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NDM, "(STUBBED) daemon_lock_enabled=0x%08X ", daemon_lock_enabled);
}

void SuspendDaemons(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 bit_mask = cmd_buff[1] & 0xF;
    daemon_bit_mask =
        static_cast<DaemonMask>(static_cast<u32>(default_daemon_bit_mask) & ~bit_mask);
    for (size_t index = 0; index < daemon_status.size(); ++index) {
        if (bit_mask & (1 << index)) {
            daemon_status[index] = DaemonStatus::Suspended;
        }
    }

    cmd_buff[0] = IPC::MakeHeader(0x6, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NDM, "(STUBBED) daemon_bit_mask=0x%08X ", daemon_bit_mask);
}

void ResumeDaemons(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 bit_mask = cmd_buff[1] & 0xF;
    daemon_bit_mask = static_cast<DaemonMask>(static_cast<u32>(daemon_bit_mask) | bit_mask);
    for (size_t index = 0; index < daemon_status.size(); ++index) {
        if (bit_mask & (1 << index)) {
            daemon_status[index] = DaemonStatus::Idle;
        }
    }

    cmd_buff[0] = IPC::MakeHeader(0x7, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NDM, "(STUBBED) daemon_bit_mask=0x%08X ", daemon_bit_mask);
}

void SuspendScheduler(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x8, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NDM, "(STUBBED) called");
}

void ResumeScheduler(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x9, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NDM, "(STUBBED) called");
}

void QueryStatus(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 daemon = cmd_buff[1] & 0xF;

    cmd_buff[0] = IPC::MakeHeader(0xD, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = static_cast<u32>(daemon_status.at(daemon));
    LOG_WARNING(Service_NDM, "(STUBBED) daemon=0x%08X, daemon_status=0x%08X", daemon, cmd_buff[2]);
}

void GetDaemonDisableCount(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 daemon = cmd_buff[1] & 0xF;

    cmd_buff[0] = IPC::MakeHeader(0xE, 3, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0;
    cmd_buff[3] = 0;
    LOG_WARNING(Service_NDM, "(STUBBED) daemon=0x%08X", daemon);
}

void GetSchedulerDisableCount(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0xF, 3, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = 0;
    cmd_buff[3] = 0;
    LOG_WARNING(Service_NDM, "(STUBBED) called");
}

void SetScanInterval(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    scan_interval = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0x10, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NDM, "(STUBBED) scan_interval=0x%08X ", scan_interval);
}

void GetScanInterval(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x11, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = scan_interval;
    LOG_WARNING(Service_NDM, "(STUBBED) scan_interval=0x%08X ", scan_interval);
}

void SetRetryInterval(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    retry_interval = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0x12, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NDM, "(STUBBED) retry_interval=0x%08X ", retry_interval);
}

void GetRetryInterval(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x13, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = retry_interval;
    LOG_WARNING(Service_NDM, "(STUBBED) retry_interval=0x%08X ", retry_interval);
}

void OverrideDefaultDaemons(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    u32 bit_mask = cmd_buff[1] & 0xF;
    default_daemon_bit_mask = static_cast<DaemonMask>(bit_mask);
    daemon_bit_mask = default_daemon_bit_mask;
    for (size_t index = 0; index < daemon_status.size(); ++index) {
        if (bit_mask & (1 << index)) {
            daemon_status[index] = DaemonStatus::Idle;
        }
    }

    cmd_buff[0] = IPC::MakeHeader(0x14, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NDM, "(STUBBED) default_daemon_bit_mask=0x%08X ", default_daemon_bit_mask);
}

void ResetDefaultDaemons(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    default_daemon_bit_mask = DaemonMask::Default;

    cmd_buff[0] = IPC::MakeHeader(0x15, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NDM, "(STUBBED) default_daemon_bit_mask=0x%08X ", default_daemon_bit_mask);
}

void GetDefaultDaemons(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x16, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    cmd_buff[2] = static_cast<u32>(default_daemon_bit_mask);
    LOG_WARNING(Service_NDM, "(STUBBED) default_daemon_bit_mask=0x%08X ", default_daemon_bit_mask);
}

void ClearHalfAwakeMacFilter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x17, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw; // No error
    LOG_WARNING(Service_NDM, "(STUBBED) called");
}

void Init() {
    AddService(new NDM_U_Interface);
}

void Shutdown() {}

} // namespace NDM
} // namespace Service
