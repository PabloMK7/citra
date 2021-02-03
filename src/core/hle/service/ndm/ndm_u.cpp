// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/core.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/service/ndm/ndm_u.h"

SERIALIZE_EXPORT_IMPL(Service::NDM::NDM_U)

namespace Service::NDM {

void NDM_U::EnterExclusiveState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 1, 2);
    exclusive_state = rp.PopEnum<ExclusiveState>();
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) exclusive_state=0x{:08X}", exclusive_state);
}

void NDM_U::LeaveExclusiveState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 0, 2);
    rp.PopPID();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::QueryExclusiveMode(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(exclusive_state);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::LockState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x04, 0, 2);
    rp.PopPID();
    daemon_lock_enabled = true;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::UnlockState(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x05, 0, 2);
    rp.PopPID();
    daemon_lock_enabled = false;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::SuspendDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x06, 1, 0);
    u32 bit_mask = rp.Pop<u32>() & 0xF;
    daemon_bit_mask =
        static_cast<DaemonMask>(static_cast<u32>(default_daemon_bit_mask) & ~bit_mask);
    for (std::size_t index = 0; index < daemon_status.size(); ++index) {
        if (bit_mask & (1 << index)) {
            daemon_status[index] = DaemonStatus::Suspended;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) bit_mask=0x{:08X}", bit_mask);
}

void NDM_U::ResumeDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x07, 1, 0);
    u32 bit_mask = rp.Pop<u32>() & 0xF;
    daemon_bit_mask = static_cast<DaemonMask>(static_cast<u32>(daemon_bit_mask) & ~bit_mask);
    for (std::size_t index = 0; index < daemon_status.size(); ++index) {
        if (bit_mask & (1 << index)) {
            daemon_status[index] = DaemonStatus::Idle;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) bit_mask=0x{:08X}", bit_mask);
}

void NDM_U::SuspendScheduler(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x08, 1, 0);
    bool perform_in_background = rp.Pop<bool>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) perform_in_background={}", perform_in_background);
}

void NDM_U::ResumeScheduler(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x09, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::QueryStatus(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0D, 1, 0);
    u8 daemon = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(daemon_status.at(daemon));
    LOG_WARNING(Service_NDM, "(STUBBED) daemon=0x{:02X}", daemon);
}

void NDM_U::GetDaemonDisableCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0E, 1, 0);
    u8 daemon = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // current process disable count
    rb.Push<u32>(0); // total disable count
    LOG_WARNING(Service_NDM, "(STUBBED) daemon=0x{:02X}", daemon);
}

void NDM_U::GetSchedulerDisableCount(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0F, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(3, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0); // current process disable count
    rb.Push<u32>(0); // total disable count
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::SetScanInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 1, 0);
    scan_interval = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) scan_interval=0x{:08X}", scan_interval);
}

void NDM_U::GetScanInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(scan_interval);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::SetRetryInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x12, 1, 0);
    retry_interval = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) retry_interval=0x{:08X}", retry_interval);
}

void NDM_U::GetRetryInterval(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x13, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(retry_interval);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::OverrideDefaultDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x14, 1, 0);
    u32 bit_mask = rp.Pop<u32>() & 0xF;
    default_daemon_bit_mask = static_cast<DaemonMask>(bit_mask);
    daemon_bit_mask = default_daemon_bit_mask;
    for (std::size_t index = 0; index < daemon_status.size(); ++index) {
        if (bit_mask & (1 << index)) {
            daemon_status[index] = DaemonStatus::Idle;
        }
    }

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED) bit_mask=0x{:08X}", bit_mask);
}

void NDM_U::ResetDefaultDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x15, 0, 0);
    default_daemon_bit_mask = DaemonMask::Default;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::GetDefaultDaemons(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x16, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(default_daemon_bit_mask);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

void NDM_U::ClearHalfAwakeMacFilter(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x17, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);
    LOG_WARNING(Service_NDM, "(STUBBED)");
}

NDM_U::NDM_U() : ServiceFramework("ndm:u", 6) {
    static const FunctionInfo functions[] = {
        {0x00010042, &NDM_U::EnterExclusiveState, "EnterExclusiveState"},
        {0x00020002, &NDM_U::LeaveExclusiveState, "LeaveExclusiveState"},
        {0x00030000, &NDM_U::QueryExclusiveMode, "QueryExclusiveMode"},
        {0x00040002, &NDM_U::LockState, "LockState"},
        {0x00050002, &NDM_U::UnlockState, "UnlockState"},
        {0x00060040, &NDM_U::SuspendDaemons, "SuspendDaemons"},
        {0x00070040, &NDM_U::ResumeDaemons, "ResumeDaemons"},
        {0x00080040, &NDM_U::SuspendScheduler, "SuspendScheduler"},
        {0x00090000, &NDM_U::ResumeScheduler, "ResumeScheduler"},
        {0x000A0000, nullptr, "GetCurrentState"},
        {0x000B0000, nullptr, "GetTargetState"},
        {0x000C0000, nullptr, "<Stubbed>"},
        {0x000D0040, &NDM_U::QueryStatus, "QueryStatus"},
        {0x000E0040, &NDM_U::GetDaemonDisableCount, "GetDaemonDisableCount"},
        {0x000F0000, &NDM_U::GetSchedulerDisableCount, "GetSchedulerDisableCount"},
        {0x00100040, &NDM_U::SetScanInterval, "SetScanInterval"},
        {0x00110000, &NDM_U::GetScanInterval, "GetScanInterval"},
        {0x00120040, &NDM_U::SetRetryInterval, "SetRetryInterval"},
        {0x00130000, &NDM_U::GetRetryInterval, "GetRetryInterval"},
        {0x00140040, &NDM_U::OverrideDefaultDaemons, "OverrideDefaultDaemons"},
        {0x00150000, &NDM_U::ResetDefaultDaemons, "ResetDefaultDaemons"},
        {0x00160000, &NDM_U::GetDefaultDaemons, "GetDefaultDaemons"},
        {0x00170000, &NDM_U::ClearHalfAwakeMacFilter, "ClearHalfAwakeMacFilter"},
    };
    RegisterHandlers(functions);
}

void InstallInterfaces(Core::System& system) {
    auto& service_manager = system.ServiceManager();
    std::make_shared<NDM_U>()->InstallAsService(service_manager);
}

} // namespace Service::NDM
