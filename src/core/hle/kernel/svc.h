// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/svc_wrapper.h"
#include "core/hle/result.h"

class ARM_Interface;

namespace Core {
class System;
} // namespace Core

namespace Kernel {

class KernelSystem;

struct MemoryInfo {
    u32 base_address;
    u32 size;
    u32 permission;
    u32 state;
};

struct PageInfo {
    u32 flags;
};

/// Values accepted by svcGetSystemInfo's type parameter.
enum class SystemInfoType {
    /**
     * Reports total used memory for all regions or a specific one, according to the extra
     * parameter. See `SystemInfoMemUsageRegion`.
     */
    REGION_MEMORY_USAGE = 0,
    /**
     * Returns the memory usage for certain allocations done internally by the kernel.
     */
    KERNEL_ALLOCATED_PAGES = 2,
    /**
     * "This returns the total number of processes which were launched directly by the kernel.
     * For the ARM11 NATIVE_FIRM kernel, this is 5, for processes sm, fs, pm, loader, and pxi."
     */
    KERNEL_SPAWNED_PIDS = 26,
};

/**
 * Accepted by svcGetSystemInfo param with REGION_MEMORY_USAGE type. Selects a region to query
 * memory usage of.
 */
enum class SystemInfoMemUsageRegion {
    ALL = 0,
    APPLICATION = 1,
    SYSTEM = 2,
    BASE = 3,
};

class SVC : public SVCWrapper<SVC> {
public:
    SVC(Core::System& system);
    void CallSVC(u32 immediate);

private:
    Core::System& system;
    Kernel::KernelSystem& kernel;

    friend class SVCWrapper<SVC>;

    // ARM interfaces

    u32 GetReg(std::size_t n);
    void SetReg(std::size_t n, u32 value);

    // SVC interfaces

    ResultCode ControlMemory(u32* out_addr, u32 addr0, u32 addr1, u32 size, u32 operation,
                             u32 permissions);
    void ExitProcess();
    ResultCode MapMemoryBlock(Handle handle, u32 addr, u32 permissions, u32 other_permissions);
    ResultCode UnmapMemoryBlock(Handle handle, u32 addr);
    ResultCode ConnectToPort(Handle* out_handle, VAddr port_name_address);
    ResultCode SendSyncRequest(Handle handle);
    ResultCode CloseHandle(Handle handle);
    ResultCode WaitSynchronization1(Handle handle, s64 nano_seconds);
    ResultCode WaitSynchronizationN(s32* out, VAddr handles_address, s32 handle_count,
                                    bool wait_all, s64 nano_seconds);
    ResultCode ReplyAndReceive(s32* index, VAddr handles_address, s32 handle_count,
                               Handle reply_target);
    ResultCode CreateAddressArbiter(Handle* out_handle);
    ResultCode ArbitrateAddress(Handle handle, u32 address, u32 type, u32 value, s64 nanoseconds);
    void Break(u8 break_reason);
    void OutputDebugString(VAddr address, s32 len);
    ResultCode GetResourceLimit(Handle* resource_limit, Handle process_handle);
    ResultCode GetResourceLimitCurrentValues(VAddr values, Handle resource_limit_handle,
                                             VAddr names, u32 name_count);
    ResultCode GetResourceLimitLimitValues(VAddr values, Handle resource_limit_handle, VAddr names,
                                           u32 name_count);
    ResultCode CreateThread(Handle* out_handle, u32 entry_point, u32 arg, VAddr stack_top,
                            u32 priority, s32 processor_id);
    void ExitThread();
    ResultCode GetThreadPriority(u32* priority, Handle handle);
    ResultCode SetThreadPriority(Handle handle, u32 priority);
    ResultCode CreateMutex(Handle* out_handle, u32 initial_locked);
    ResultCode ReleaseMutex(Handle handle);
    ResultCode GetProcessId(u32* process_id, Handle process_handle);
    ResultCode GetProcessIdOfThread(u32* process_id, Handle thread_handle);
    ResultCode GetThreadId(u32* thread_id, Handle handle);
    ResultCode CreateSemaphore(Handle* out_handle, s32 initial_count, s32 max_count);
    ResultCode ReleaseSemaphore(s32* count, Handle handle, s32 release_count);
    ResultCode QueryProcessMemory(MemoryInfo* memory_info, PageInfo* page_info,
                                  Handle process_handle, u32 addr);
    ResultCode QueryMemory(MemoryInfo* memory_info, PageInfo* page_info, u32 addr);
    ResultCode CreateEvent(Handle* out_handle, u32 reset_type);
    ResultCode DuplicateHandle(Handle* out, Handle handle);
    ResultCode SignalEvent(Handle handle);
    ResultCode ClearEvent(Handle handle);
    ResultCode CreateTimer(Handle* out_handle, u32 reset_type);
    ResultCode ClearTimer(Handle handle);
    ResultCode SetTimer(Handle handle, s64 initial, s64 interval);
    ResultCode CancelTimer(Handle handle);
    void SleepThread(s64 nanoseconds);
    s64 GetSystemTick();
    ResultCode CreateMemoryBlock(Handle* out_handle, u32 addr, u32 size, u32 my_permission,
                                 u32 other_permission);
    ResultCode CreatePort(Handle* server_port, Handle* client_port, VAddr name_address,
                          u32 max_sessions);
    ResultCode CreateSessionToPort(Handle* out_client_session, Handle client_port_handle);
    ResultCode CreateSession(Handle* server_session, Handle* client_session);
    ResultCode AcceptSession(Handle* out_server_session, Handle server_port_handle);
    ResultCode GetSystemInfo(s64* out, u32 type, s32 param);
    ResultCode GetProcessInfo(s64* out, Handle process_handle, u32 type);

    struct FunctionDef {
        using Func = void (SVC::*)();

        u32 id;
        Func func;
        const char* name;
    };

    static const FunctionDef SVC_Table[];
    static const FunctionDef* GetSVCInfo(u32 func_num);
};

} // namespace Kernel
