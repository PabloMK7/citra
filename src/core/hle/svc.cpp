// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>

#include "common/string_util.h"
#include "common/symbols.h"

#include "core/mem_map.h"

#include "core/hle/kernel/address_arbiter.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/semaphore.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/kernel/thread.h"

#include "core/hle/function_wrappers.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SVC

namespace SVC {

enum ControlMemoryOperation {
    MEMORY_OPERATION_HEAP       = 0x00000003,
    MEMORY_OPERATION_GSP_HEAP   = 0x00010003,
};

/// Map application or GSP heap memory
static Result ControlMemory(u32* out_addr, u32 operation, u32 addr0, u32 addr1, u32 size, u32 permissions) {
    LOG_TRACE(Kernel_SVC,"called operation=0x%08X, addr0=0x%08X, addr1=0x%08X, size=%08X, permissions=0x%08X",
        operation, addr0, addr1, size, permissions);

    switch (operation) {

    // Map normal heap memory
    case MEMORY_OPERATION_HEAP:
        *out_addr = Memory::MapBlock_Heap(size, operation, permissions);
        break;

    // Map GSP heap memory
    case MEMORY_OPERATION_GSP_HEAP:
        *out_addr = Memory::MapBlock_HeapLinear(size, operation, permissions);
        break;

    // Unknown ControlMemory operation
    default:
        LOG_ERROR(Kernel_SVC, "unknown operation=0x%08X", operation);
    }
    return 0;
}

/// Maps a memory block to specified address
static Result MapMemoryBlock(Handle handle, u32 addr, u32 permissions, u32 other_permissions) {
    LOG_TRACE(Kernel_SVC, "called memblock=0x%08X, addr=0x%08X, mypermissions=0x%08X, otherpermission=%d",
        handle, addr, permissions, other_permissions);

    Kernel::MemoryPermission permissions_type = static_cast<Kernel::MemoryPermission>(permissions);
    switch (permissions_type) {
    case Kernel::MemoryPermission::Read:
    case Kernel::MemoryPermission::Write:
    case Kernel::MemoryPermission::ReadWrite:
    case Kernel::MemoryPermission::Execute:
    case Kernel::MemoryPermission::ReadExecute:
    case Kernel::MemoryPermission::WriteExecute:
    case Kernel::MemoryPermission::ReadWriteExecute:
    case Kernel::MemoryPermission::DontCare:
        Kernel::MapSharedMemory(handle, addr, permissions_type,
            static_cast<Kernel::MemoryPermission>(other_permissions));
        break;
    default:
        LOG_ERROR(Kernel_SVC, "unknown permissions=0x%08X", permissions);
    }
    return 0;
}

/// Connect to an OS service given the port name, returns the handle to the port to out
static Result ConnectToPort(Handle* out, const char* port_name) {
    Service::Interface* service = Service::g_manager->FetchFromPortName(port_name);

    LOG_TRACE(Kernel_SVC, "called port_name=%s", port_name);
    _assert_msg_(KERNEL, (service != nullptr), "called, but service is not implemented!");

    *out = service->GetHandle();

    return 0;
}

/// Synchronize to an OS service
static Result SendSyncRequest(Handle handle) {
    Kernel::Session* session = Kernel::g_handle_table.Get<Kernel::Session>(handle);
    if (session == nullptr) {
        return InvalidHandle(ErrorModule::Kernel).raw;
    }

    LOG_TRACE(Kernel_SVC, "called handle=0x%08X(%s)", handle, session->GetName().c_str());

    ResultVal<bool> wait = session->SyncRequest();
    if (wait.Succeeded() && *wait) {
        Kernel::WaitCurrentThread(WAITTYPE_SYNCH); // TODO(bunnei): Is this correct?
    }

    return wait.Code().raw;
}

/// Close a handle
static Result CloseHandle(Handle handle) {
    // ImplementMe
    LOG_ERROR(Kernel_SVC, "(UNIMPLEMENTED) called handle=0x%08X", handle);
    return 0;
}

/// Wait for a handle to synchronize, timeout after the specified nanoseconds
static Result WaitSynchronization1(Handle handle, s64 nano_seconds) {
    // TODO(bunnei): Do something with nano_seconds, currently ignoring this
    bool wait_infinite = (nano_seconds == -1); // Used to wait until a thread has terminated

    Kernel::Object* object = Kernel::g_handle_table.GetGeneric(handle);
    if (object == nullptr)
        return InvalidHandle(ErrorModule::Kernel).raw;

    LOG_TRACE(Kernel_SVC, "called handle=0x%08X(%s:%s), nanoseconds=%lld", handle, object->GetTypeName().c_str(),
            object->GetName().c_str(), nano_seconds);

    ResultVal<bool> wait = object->WaitSynchronization();

    // Check for next thread to schedule
    if (wait.Succeeded() && *wait) {
        HLE::Reschedule(__func__);
    }

    return wait.Code().raw;
}

/// Wait for the given handles to synchronize, timeout after the specified nanoseconds
static Result WaitSynchronizationN(s32* out, Handle* handles, s32 handle_count, bool wait_all,
    s64 nano_seconds) {
    // TODO(bunnei): Do something with nano_seconds, currently ignoring this
    bool unlock_all = true;
    bool wait_infinite = (nano_seconds == -1); // Used to wait until a thread has terminated

    LOG_TRACE(Kernel_SVC, "called handle_count=%d, wait_all=%s, nanoseconds=%lld",
        handle_count, (wait_all ? "true" : "false"), nano_seconds);

    // Iterate through each handle, synchronize kernel object
    for (s32 i = 0; i < handle_count; i++) {
        Kernel::Object* object = Kernel::g_handle_table.GetGeneric(handles[i]);
        if (object == nullptr)
            return InvalidHandle(ErrorModule::Kernel).raw;

        LOG_TRACE(Kernel_SVC, "\thandle[%d] = 0x%08X(%s:%s)", i, handles[i], object->GetTypeName().c_str(),
            object->GetName().c_str());

        // TODO(yuriks): Verify how the real function behaves when an error happens here
        ResultVal<bool> wait_result = object->WaitSynchronization();
        bool wait = wait_result.Succeeded() && *wait_result;

        if (!wait && !wait_all) {
            *out = i;
            return RESULT_SUCCESS.raw;
        } else {
            unlock_all = false;
        }
    }

    if (wait_all && unlock_all) {
        *out = handle_count;
        return RESULT_SUCCESS.raw;
    }

    // Check for next thread to schedule
    HLE::Reschedule(__func__);

    return RESULT_SUCCESS.raw;
}

/// Create an address arbiter (to allocate access to shared resources)
static Result CreateAddressArbiter(u32* arbiter) {
    LOG_TRACE(Kernel_SVC, "called");
    Handle handle = Kernel::CreateAddressArbiter();
    *arbiter = handle;
    return 0;
}

/// Arbitrate address
static Result ArbitrateAddress(Handle arbiter, u32 address, u32 type, u32 value, s64 nanoseconds) {
    LOG_TRACE(Kernel_SVC, "called handle=0x%08X, address=0x%08X, type=0x%08X, value=0x%08X", arbiter,
        address, type, value);
    return Kernel::ArbitrateAddress(arbiter, static_cast<Kernel::ArbitrationType>(type),
            address, value).raw;
}

/// Used to output a message on a debug hardware unit - does nothing on a retail unit
static void OutputDebugString(const char* string) {
    LOG_DEBUG(Debug_Emulated, "%s", string);
}

/// Get resource limit
static Result GetResourceLimit(Handle* resource_limit, Handle process) {
    // With regards to proceess values:
    // 0xFFFF8001 is a handle alias for the current KProcess, and 0xFFFF8000 is a handle alias for
    // the current KThread.
    *resource_limit = 0xDEADBEEF;
    LOG_ERROR(Kernel_SVC, "(UNIMPLEMENTED) called process=0x%08X", process);
    return 0;
}

/// Get resource limit current values
static Result GetResourceLimitCurrentValues(s64* values, Handle resource_limit, void* names,
    s32 name_count) {
    LOG_ERROR(Kernel_SVC, "(UNIMPLEMENTED) called resource_limit=%08X, names=%s, name_count=%d",
        resource_limit, names, name_count);
    Memory::Write32(Core::g_app_core->GetReg(0), 0); // Normmatt: Set used memory to 0 for now
    return 0;
}

/// Creates a new thread
static Result CreateThread(u32 priority, u32 entry_point, u32 arg, u32 stack_top, u32 processor_id) {
    std::string name;
    if (Symbols::HasSymbol(entry_point)) {
        TSymbol symbol = Symbols::GetSymbol(entry_point);
        name = symbol.name;
    } else {
        name = Common::StringFromFormat("unknown-%08x", entry_point);
    }

    Handle thread = Kernel::CreateThread(name.c_str(), entry_point, priority, arg, processor_id,
        stack_top);

    Core::g_app_core->SetReg(1, thread);

    LOG_TRACE(Kernel_SVC, "called entrypoint=0x%08X (%s), arg=0x%08X, stacktop=0x%08X, "
        "threadpriority=0x%08X, processorid=0x%08X : created handle=0x%08X", entry_point,
        name.c_str(), arg, stack_top, priority, processor_id, thread);

    return 0;
}

/// Called when a thread exits
static u32 ExitThread() {
    Handle thread = Kernel::GetCurrentThreadHandle();

    LOG_TRACE(Kernel_SVC, "called, pc=0x%08X", Core::g_app_core->GetPC()); // PC = 0x0010545C

    Kernel::StopThread(thread, __func__);
    HLE::Reschedule(__func__);
    return 0;
}

/// Gets the priority for the specified thread
static Result GetThreadPriority(s32* priority, Handle handle) {
    ResultVal<u32> priority_result = Kernel::GetThreadPriority(handle);
    if (priority_result.Succeeded()) {
        *priority = *priority_result;
    }
    return priority_result.Code().raw;
}

/// Sets the priority for the specified thread
static Result SetThreadPriority(Handle handle, s32 priority) {
    return Kernel::SetThreadPriority(handle, priority).raw;
}

/// Create a mutex
static Result CreateMutex(Handle* mutex, u32 initial_locked) {
    *mutex = Kernel::CreateMutex((initial_locked != 0));
    LOG_TRACE(Kernel_SVC, "called initial_locked=%s : created handle=0x%08X",
        initial_locked ? "true" : "false", *mutex);
    return 0;
}

/// Release a mutex
static Result ReleaseMutex(Handle handle) {
    LOG_TRACE(Kernel_SVC, "called handle=0x%08X", handle);
    ResultCode res = Kernel::ReleaseMutex(handle);
    return res.raw;
}

/// Get the ID for the specified thread.
static Result GetThreadId(u32* thread_id, Handle handle) {
    LOG_TRACE(Kernel_SVC, "called thread=0x%08X", handle);
    ResultCode result = Kernel::GetThreadId(thread_id, handle);
    return result.raw;
}

/// Creates a semaphore
static Result CreateSemaphore(Handle* semaphore, s32 initial_count, s32 max_count) {
    ResultCode res = Kernel::CreateSemaphore(semaphore, initial_count, max_count);
    LOG_TRACE(Kernel_SVC, "called initial_count=%d, max_count=%d, created handle=0x%08X",
        initial_count, max_count, *semaphore);
    return res.raw;
}

/// Releases a certain number of slots in a semaphore
static Result ReleaseSemaphore(s32* count, Handle semaphore, s32 release_count) {
    LOG_TRACE(Kernel_SVC, "called release_count=%d, handle=0x%08X", release_count, semaphore);
    ResultCode res = Kernel::ReleaseSemaphore(count, semaphore, release_count);
    return res.raw;
}

/// Query memory
static Result QueryMemory(void* info, void* out, u32 addr) {
    LOG_ERROR(Kernel_SVC, "(UNIMPLEMENTED) called addr=0x%08X", addr);
    return 0;
}

/// Create an event
static Result CreateEvent(Handle* evt, u32 reset_type) {
    *evt = Kernel::CreateEvent((ResetType)reset_type);
    LOG_TRACE(Kernel_SVC, "called reset_type=0x%08X : created handle=0x%08X",
        reset_type, *evt);
    return 0;
}

/// Duplicates a kernel handle
static Result DuplicateHandle(Handle* out, Handle handle) {
    ResultVal<Handle> out_h = Kernel::g_handle_table.Duplicate(handle);
    if (out_h.Succeeded()) {
        *out = *out_h;
        LOG_TRACE(Kernel_SVC, "duplicated 0x%08X to 0x%08X", handle, *out);
    }
    return out_h.Code().raw;
}

/// Signals an event
static Result SignalEvent(Handle evt) {
    LOG_TRACE(Kernel_SVC, "called event=0x%08X", evt);
    return Kernel::SignalEvent(evt).raw;
}

/// Clears an event
static Result ClearEvent(Handle evt) {
    LOG_TRACE(Kernel_SVC, "called event=0x%08X", evt);
    return Kernel::ClearEvent(evt).raw;
}

/// Sleep the current thread
static void SleepThread(s64 nanoseconds) {
    LOG_TRACE(Kernel_SVC, "called nanoseconds=%lld", nanoseconds);

    // Sleep current thread and check for next thread to schedule
    Kernel::WaitCurrentThread(WAITTYPE_SLEEP);
    HLE::Reschedule(__func__);
}

/// This returns the total CPU ticks elapsed since the CPU was powered-on
static s64 GetSystemTick() {
    return (s64)Core::g_app_core->GetTicks();
}

/// Creates a memory block at the specified address with the specified permissions and size
static Result CreateMemoryBlock(Handle* memblock, u32 addr, u32 size, u32 my_permission,
    u32 other_permission) {

    // TODO(Subv): Implement this function

    Handle shared_memory = Kernel::CreateSharedMemory();
    *memblock = shared_memory;
    LOG_WARNING(Kernel_SVC, "(STUBBED) called addr=0x%08X", addr);
    return 0;
}

const HLE::FunctionDef SVC_Table[] = {
    {0x00, nullptr,                         "Unknown"},
    {0x01, HLE::Wrap<ControlMemory>,        "ControlMemory"},
    {0x02, HLE::Wrap<QueryMemory>,          "QueryMemory"},
    {0x03, nullptr,                         "ExitProcess"},
    {0x04, nullptr,                         "GetProcessAffinityMask"},
    {0x05, nullptr,                         "SetProcessAffinityMask"},
    {0x06, nullptr,                         "GetProcessIdealProcessor"},
    {0x07, nullptr,                         "SetProcessIdealProcessor"},
    {0x08, HLE::Wrap<CreateThread>,         "CreateThread"},
    {0x09, HLE::Wrap<ExitThread>,           "ExitThread"},
    {0x0A, HLE::Wrap<SleepThread>,          "SleepThread"},
    {0x0B, HLE::Wrap<GetThreadPriority>,    "GetThreadPriority"},
    {0x0C, HLE::Wrap<SetThreadPriority>,    "SetThreadPriority"},
    {0x0D, nullptr,                         "GetThreadAffinityMask"},
    {0x0E, nullptr,                         "SetThreadAffinityMask"},
    {0x0F, nullptr,                         "GetThreadIdealProcessor"},
    {0x10, nullptr,                         "SetThreadIdealProcessor"},
    {0x11, nullptr,                         "GetCurrentProcessorNumber"},
    {0x12, nullptr,                         "Run"},
    {0x13, HLE::Wrap<CreateMutex>,          "CreateMutex"},
    {0x14, HLE::Wrap<ReleaseMutex>,         "ReleaseMutex"},
    {0x15, HLE::Wrap<CreateSemaphore>,      "CreateSemaphore"},
    {0x16, HLE::Wrap<ReleaseSemaphore>,     "ReleaseSemaphore"},
    {0x17, HLE::Wrap<CreateEvent>,          "CreateEvent"},
    {0x18, HLE::Wrap<SignalEvent>,          "SignalEvent"},
    {0x19, HLE::Wrap<ClearEvent>,           "ClearEvent"},
    {0x1A, nullptr,                         "CreateTimer"},
    {0x1B, nullptr,                         "SetTimer"},
    {0x1C, nullptr,                         "CancelTimer"},
    {0x1D, nullptr,                         "ClearTimer"},
    {0x1E, HLE::Wrap<CreateMemoryBlock>,    "CreateMemoryBlock"},
    {0x1F, HLE::Wrap<MapMemoryBlock>,       "MapMemoryBlock"},
    {0x20, nullptr,                         "UnmapMemoryBlock"},
    {0x21, HLE::Wrap<CreateAddressArbiter>, "CreateAddressArbiter"},
    {0x22, HLE::Wrap<ArbitrateAddress>,     "ArbitrateAddress"},
    {0x23, HLE::Wrap<CloseHandle>,          "CloseHandle"},
    {0x24, HLE::Wrap<WaitSynchronization1>, "WaitSynchronization1"},
    {0x25, HLE::Wrap<WaitSynchronizationN>, "WaitSynchronizationN"},
    {0x26, nullptr,                         "SignalAndWait"},
    {0x27, HLE::Wrap<DuplicateHandle>,      "DuplicateHandle"},
    {0x28, HLE::Wrap<GetSystemTick>,        "GetSystemTick"},
    {0x29, nullptr,                         "GetHandleInfo"},
    {0x2A, nullptr,                         "GetSystemInfo"},
    {0x2B, nullptr,                         "GetProcessInfo"},
    {0x2C, nullptr,                         "GetThreadInfo"},
    {0x2D, HLE::Wrap<ConnectToPort>,        "ConnectToPort"},
    {0x2E, nullptr,                         "SendSyncRequest1"},
    {0x2F, nullptr,                         "SendSyncRequest2"},
    {0x30, nullptr,                         "SendSyncRequest3"},
    {0x31, nullptr,                         "SendSyncRequest4"},
    {0x32, HLE::Wrap<SendSyncRequest>,      "SendSyncRequest"},
    {0x33, nullptr,                         "OpenProcess"},
    {0x34, nullptr,                         "OpenThread"},
    {0x35, nullptr,                         "GetProcessId"},
    {0x36, nullptr,                         "GetProcessIdOfThread"},
    {0x37, HLE::Wrap<GetThreadId>,          "GetThreadId"},
    {0x38, HLE::Wrap<GetResourceLimit>,     "GetResourceLimit"},
    {0x39, nullptr,                         "GetResourceLimitLimitValues"},
    {0x3A, HLE::Wrap<GetResourceLimitCurrentValues>, "GetResourceLimitCurrentValues"},
    {0x3B, nullptr,                         "GetThreadContext"},
    {0x3C, nullptr,                         "Break"},
    {0x3D, HLE::Wrap<OutputDebugString>,    "OutputDebugString"},
    {0x3E, nullptr,                         "ControlPerformanceCounter"},
    {0x3F, nullptr,                         "Unknown"},
    {0x40, nullptr,                         "Unknown"},
    {0x41, nullptr,                         "Unknown"},
    {0x42, nullptr,                         "Unknown"},
    {0x43, nullptr,                         "Unknown"},
    {0x44, nullptr,                         "Unknown"},
    {0x45, nullptr,                         "Unknown"},
    {0x46, nullptr,                         "Unknown"},
    {0x47, nullptr,                         "CreatePort"},
    {0x48, nullptr,                         "CreateSessionToPort"},
    {0x49, nullptr,                         "CreateSession"},
    {0x4A, nullptr,                         "AcceptSession"},
    {0x4B, nullptr,                         "ReplyAndReceive1"},
    {0x4C, nullptr,                         "ReplyAndReceive2"},
    {0x4D, nullptr,                         "ReplyAndReceive3"},
    {0x4E, nullptr,                         "ReplyAndReceive4"},
    {0x4F, nullptr,                         "ReplyAndReceive"},
    {0x50, nullptr,                         "BindInterrupt"},
    {0x51, nullptr,                         "UnbindInterrupt"},
    {0x52, nullptr,                         "InvalidateProcessDataCache"},
    {0x53, nullptr,                         "StoreProcessDataCache"},
    {0x54, nullptr,                         "FlushProcessDataCache"},
    {0x55, nullptr,                         "StartInterProcessDma"},
    {0x56, nullptr,                         "StopDma"},
    {0x57, nullptr,                         "GetDmaState"},
    {0x58, nullptr,                         "RestartDma"},
    {0x59, nullptr,                         "Unknown"},
    {0x5A, nullptr,                         "Unknown"},
    {0x5B, nullptr,                         "Unknown"},
    {0x5C, nullptr,                         "Unknown"},
    {0x5D, nullptr,                         "Unknown"},
    {0x5E, nullptr,                         "Unknown"},
    {0x5F, nullptr,                         "Unknown"},
    {0x60, nullptr,                         "DebugActiveProcess"},
    {0x61, nullptr,                         "BreakDebugProcess"},
    {0x62, nullptr,                         "TerminateDebugProcess"},
    {0x63, nullptr,                         "GetProcessDebugEvent"},
    {0x64, nullptr,                         "ContinueDebugEvent"},
    {0x65, nullptr,                         "GetProcessList"},
    {0x66, nullptr,                         "GetThreadList"},
    {0x67, nullptr,                         "GetDebugThreadContext"},
    {0x68, nullptr,                         "SetDebugThreadContext"},
    {0x69, nullptr,                         "QueryDebugProcessMemory"},
    {0x6A, nullptr,                         "ReadProcessMemory"},
    {0x6B, nullptr,                         "WriteProcessMemory"},
    {0x6C, nullptr,                         "SetHardwareBreakPoint"},
    {0x6D, nullptr,                         "GetDebugThreadParam"},
    {0x6E, nullptr,                         "Unknown"},
    {0x6F, nullptr,                         "Unknown"},
    {0x70, nullptr,                         "ControlProcessMemory"},
    {0x71, nullptr,                         "MapProcessMemory"},
    {0x72, nullptr,                         "UnmapProcessMemory"},
    {0x73, nullptr,                         "Unknown"},
    {0x74, nullptr,                         "Unknown"},
    {0x75, nullptr,                         "Unknown"},
    {0x76, nullptr,                         "TerminateProcess"},
    {0x77, nullptr,                         "Unknown"},
    {0x78, nullptr,                         "CreateResourceLimit"},
    {0x79, nullptr,                         "Unknown"},
    {0x7A, nullptr,                         "Unknown"},
    {0x7B, nullptr,                         "Unknown"},
    {0x7C, nullptr,                         "KernelSetState"},
    {0x7D, nullptr,                         "QueryProcessMemory"},
};

void Register() {
    HLE::RegisterModule("SVC_Table", ARRAY_SIZE(SVC_Table), SVC_Table);
}

} // namespace
