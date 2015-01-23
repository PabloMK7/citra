// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>

#include "common/string_util.h"
#include "common/symbols.h"

#include "core/arm/arm_interface.h"
#include "core/mem_map.h"

#include "core/hle/kernel/address_arbiter.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/semaphore.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"

#include "core/hle/function_wrappers.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace SVC

using Kernel::SharedPtr;

namespace SVC {

/// An invalid result code that is meant to be overwritten when a thread resumes from waiting
const ResultCode RESULT_INVALID(0xDEADC0DE);

enum ControlMemoryOperation {
    MEMORY_OPERATION_HEAP       = 0x00000003,
    MEMORY_OPERATION_GSP_HEAP   = 0x00010003,
};

/// Map application or GSP heap memory
static ResultCode ControlMemory(u32* out_addr, u32 operation, u32 addr0, u32 addr1, u32 size, u32 permissions) {
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
    return RESULT_SUCCESS;
}

/// Maps a memory block to specified address
static ResultCode MapMemoryBlock(Handle handle, u32 addr, u32 permissions, u32 other_permissions) {
    using Kernel::SharedMemory;
    using Kernel::MemoryPermission;

    LOG_TRACE(Kernel_SVC, "called memblock=0x%08X, addr=0x%08X, mypermissions=0x%08X, otherpermission=%d",
        handle, addr, permissions, other_permissions);

    SharedPtr<SharedMemory> shared_memory = Kernel::g_handle_table.Get<SharedMemory>(handle);
    if (shared_memory == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    MemoryPermission permissions_type = static_cast<MemoryPermission>(permissions);
    switch (permissions_type) {
    case MemoryPermission::Read:
    case MemoryPermission::Write:
    case MemoryPermission::ReadWrite:
    case MemoryPermission::Execute:
    case MemoryPermission::ReadExecute:
    case MemoryPermission::WriteExecute:
    case MemoryPermission::ReadWriteExecute:
    case MemoryPermission::DontCare:
        shared_memory->Map(addr, permissions_type,
                static_cast<MemoryPermission>(other_permissions));
        break;
    default:
        LOG_ERROR(Kernel_SVC, "unknown permissions=0x%08X", permissions);
    }
    return RESULT_SUCCESS;
}

/// Connect to an OS service given the port name, returns the handle to the port to out
static ResultCode ConnectToPort(Handle* out, const char* port_name) {
    Service::Interface* service = Service::g_manager->FetchFromPortName(port_name);

    LOG_TRACE(Kernel_SVC, "called port_name=%s", port_name);
    _assert_msg_(KERNEL, (service != nullptr), "called, but service is not implemented!");

    *out = service->GetHandle();

    return RESULT_SUCCESS;
}

/// Synchronize to an OS service
static ResultCode SendSyncRequest(Handle handle) {
    SharedPtr<Kernel::Session> session = Kernel::g_handle_table.Get<Kernel::Session>(handle);
    if (session == nullptr) {
        return InvalidHandle(ErrorModule::Kernel);
    }

    LOG_TRACE(Kernel_SVC, "called handle=0x%08X(%s)", handle, session->GetName().c_str());

    return session->SyncRequest().Code();
}

/// Close a handle
static ResultCode CloseHandle(Handle handle) {
    // ImplementMe
    LOG_ERROR(Kernel_SVC, "(UNIMPLEMENTED) called handle=0x%08X", handle);
    return RESULT_SUCCESS;
}

/// Wait for a handle to synchronize, timeout after the specified nanoseconds
static ResultCode WaitSynchronization1(Handle handle, s64 nano_seconds) {
    auto object = Kernel::g_handle_table.GetWaitObject(handle);
    if (object == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    LOG_TRACE(Kernel_SVC, "called handle=0x%08X(%s:%s), nanoseconds=%lld", handle,
            object->GetTypeName().c_str(), object->GetName().c_str(), nano_seconds);

    // Check for next thread to schedule
    if (object->ShouldWait()) {

        object->AddWaitingThread(Kernel::GetCurrentThread());
        Kernel::WaitCurrentThread_WaitSynchronization(object, false, false);

        // Create an event to wake the thread up after the specified nanosecond delay has passed
        Kernel::WakeThreadAfterDelay(Kernel::GetCurrentThread(), nano_seconds);

        HLE::Reschedule(__func__);

        // NOTE: output of this SVC will be set later depending on how the thread resumes
        return RESULT_INVALID;
    }

    object->Acquire();

    return RESULT_SUCCESS;
}

/// Wait for the given handles to synchronize, timeout after the specified nanoseconds
static ResultCode WaitSynchronizationN(s32* out, Handle* handles, s32 handle_count, bool wait_all, s64 nano_seconds) {
    bool wait_thread = !wait_all;
    int handle_index = 0;

    // Check if 'handles' is invalid
    if (handles == nullptr)
        return ResultCode(ErrorDescription::InvalidPointer, ErrorModule::Kernel, ErrorSummary::InvalidArgument, ErrorLevel::Permanent);

    // NOTE: on real hardware, there is no nullptr check for 'out' (tested with firmware 4.4). If
    // this happens, the running application will crash.
    _assert_msg_(Kernel, out != nullptr, "invalid output pointer specified!");

    // Check if 'handle_count' is invalid
    if (handle_count < 0)
        return ResultCode(ErrorDescription::OutOfRange, ErrorModule::OS, ErrorSummary::InvalidArgument, ErrorLevel::Usage);

    // If 'handle_count' is non-zero, iterate through each handle and wait the current thread if
    // necessary
    if (handle_count != 0) {
        bool selected = false; // True once an object has been selected
        for (int i = 0; i < handle_count; ++i) {
            auto object = Kernel::g_handle_table.GetWaitObject(handles[i]);
            if (object == nullptr)
                return InvalidHandle(ErrorModule::Kernel);

            // Check if the current thread should wait on this object...
            if (object->ShouldWait()) {

                // Check we are waiting on all objects...
                if (wait_all)
                    // Wait the thread
                    wait_thread = true;
            } else {
                // Do not wait on this object, check if this object should be selected...
                if (!wait_all && !selected) {
                    // Do not wait the thread
                    wait_thread = false;
                    handle_index = i;
                    selected = true;
                }
            }
        }
    } else {
        // If no handles were passed in, put the thread to sleep only when 'wait_all' is false
        // NOTE: This should deadlock the current thread if no timeout was specified
        if (!wait_all) {
            wait_thread = true;
            Kernel::WaitCurrentThread_WaitSynchronization(nullptr, true, wait_all);
        }
    }

    // If thread should wait, then set its state to waiting and then reschedule...
    if (wait_thread) {

        // Actually wait the current thread on each object if we decided to wait...
        for (int i = 0; i < handle_count; ++i) {
            auto object = Kernel::g_handle_table.GetWaitObject(handles[i]);
            object->AddWaitingThread(Kernel::GetCurrentThread());
            Kernel::WaitCurrentThread_WaitSynchronization(object, true, wait_all);
        }

        // Create an event to wake the thread up after the specified nanosecond delay has passed
        Kernel::WakeThreadAfterDelay(Kernel::GetCurrentThread(), nano_seconds);

        HLE::Reschedule(__func__);

        // NOTE: output of this SVC will be set later depending on how the thread resumes
        return RESULT_INVALID;
    }

    // Acquire objects if we did not wait...
    for (int i = 0; i < handle_count; ++i) {
        auto object = Kernel::g_handle_table.GetWaitObject(handles[i]);

        // Acquire the object if it is not waiting...
        if (!object->ShouldWait()) {
            object->Acquire();

            // If this was the first non-waiting object and 'wait_all' is false, don't acquire
            // any other objects
            if (!wait_all)
                break;
        }
    }

    // TODO(bunnei): If 'wait_all' is true, this is probably wrong. However, real hardware does
    // not seem to set it to any meaningful value.
    *out = wait_all ? 0 : handle_index;

    return RESULT_SUCCESS;
}

/// Create an address arbiter (to allocate access to shared resources)
static ResultCode CreateAddressArbiter(u32* arbiter) {
    using Kernel::AddressArbiter;

    ResultVal<SharedPtr<AddressArbiter>> arbiter_res = AddressArbiter::Create();
    if (arbiter_res.Failed())
        return arbiter_res.Code();

    ResultVal<Handle> handle_res = Kernel::g_handle_table.Create(*arbiter_res);
    if (handle_res.Failed())
        return handle_res.Code();

    LOG_TRACE(Kernel_SVC, "returned handle=0x%08X", *handle_res);

    *arbiter = *handle_res;
    return RESULT_SUCCESS;
}

/// Arbitrate address
static ResultCode ArbitrateAddress(Handle handle, u32 address, u32 type, u32 value, s64 nanoseconds) {
    using Kernel::AddressArbiter;

    LOG_TRACE(Kernel_SVC, "called handle=0x%08X, address=0x%08X, type=0x%08X, value=0x%08X", handle,
        address, type, value);

    SharedPtr<AddressArbiter> arbiter = Kernel::g_handle_table.Get<AddressArbiter>(handle);
    if (arbiter == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    return arbiter->ArbitrateAddress(static_cast<Kernel::ArbitrationType>(type),
            address, value, nanoseconds);
}

/// Used to output a message on a debug hardware unit - does nothing on a retail unit
static void OutputDebugString(const char* string) {
    LOG_DEBUG(Debug_Emulated, "%s", string);
}

/// Get resource limit
static ResultCode GetResourceLimit(Handle* resource_limit, Handle process) {
    // With regards to proceess values:
    // 0xFFFF8001 is a handle alias for the current KProcess, and 0xFFFF8000 is a handle alias for
    // the current KThread.
    *resource_limit = 0xDEADBEEF;
    LOG_ERROR(Kernel_SVC, "(UNIMPLEMENTED) called process=0x%08X", process);
    return RESULT_SUCCESS;
}

/// Get resource limit current values
static ResultCode GetResourceLimitCurrentValues(s64* values, Handle resource_limit, void* names,
    s32 name_count) {
    LOG_ERROR(Kernel_SVC, "(UNIMPLEMENTED) called resource_limit=%08X, names=%s, name_count=%d",
        resource_limit, names, name_count);
    Memory::Write32(Core::g_app_core->GetReg(0), 0); // Normmatt: Set used memory to 0 for now
    return RESULT_SUCCESS;
}

/// Creates a new thread
static ResultCode CreateThread(u32 priority, u32 entry_point, u32 arg, u32 stack_top, u32 processor_id) {
    using Kernel::Thread;

    std::string name;
    if (Symbols::HasSymbol(entry_point)) {
        TSymbol symbol = Symbols::GetSymbol(entry_point);
        name = symbol.name;
    } else {
        name = Common::StringFromFormat("unknown-%08x", entry_point);
    }

    ResultVal<SharedPtr<Thread>> thread_res = Kernel::Thread::Create(
            name, entry_point, priority, arg, processor_id, stack_top, Kernel::DEFAULT_STACK_SIZE);
    if (thread_res.Failed())
        return thread_res.Code();
    SharedPtr<Thread> thread = std::move(*thread_res);

    // TODO(yuriks): Create new handle instead of using built-in
    Core::g_app_core->SetReg(1, thread->GetHandle());

    LOG_TRACE(Kernel_SVC, "called entrypoint=0x%08X (%s), arg=0x%08X, stacktop=0x%08X, "
        "threadpriority=0x%08X, processorid=0x%08X : created handle=0x%08X", entry_point,
        name.c_str(), arg, stack_top, priority, processor_id, thread->GetHandle());

    if (THREADPROCESSORID_1 == processor_id) {
        LOG_WARNING(Kernel_SVC,
            "thread designated for system CPU core (UNIMPLEMENTED) will be run with app core scheduling");
    }

    return RESULT_SUCCESS;
}

/// Called when a thread exits
static void ExitThread() {
    LOG_TRACE(Kernel_SVC, "called, pc=0x%08X", Core::g_app_core->GetPC());

    Kernel::GetCurrentThread()->Stop(__func__);
    HLE::Reschedule(__func__);
}

/// Gets the priority for the specified thread
static ResultCode GetThreadPriority(s32* priority, Handle handle) {
    const SharedPtr<Kernel::Thread> thread = Kernel::g_handle_table.Get<Kernel::Thread>(handle);
    if (thread == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    *priority = thread->GetPriority();
    return RESULT_SUCCESS;
}

/// Sets the priority for the specified thread
static ResultCode SetThreadPriority(Handle handle, s32 priority) {
    SharedPtr<Kernel::Thread> thread = Kernel::g_handle_table.Get<Kernel::Thread>(handle);
    if (thread == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    thread->SetPriority(priority);
    return RESULT_SUCCESS;
}

/// Create a mutex
static ResultCode CreateMutex(Handle* handle, u32 initial_locked) {
    using Kernel::Mutex;

    auto mutex_res = Mutex::Create(initial_locked != 0);
    if (mutex_res.Failed())
        return mutex_res.Code();
    SharedPtr<Mutex> mutex = mutex_res.MoveFrom();

    *handle = Kernel::g_handle_table.Create(mutex).MoveFrom();
    LOG_TRACE(Kernel_SVC, "called initial_locked=%s : created handle=0x%08X",
        initial_locked ? "true" : "false", *handle);
    return RESULT_SUCCESS;
}

/// Release a mutex
static ResultCode ReleaseMutex(Handle handle) {
    using Kernel::Mutex;

    LOG_TRACE(Kernel_SVC, "called handle=0x%08X", handle);

    SharedPtr<Mutex> mutex = Kernel::g_handle_table.Get<Mutex>(handle);
    if (mutex == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    mutex->Release();
    return RESULT_SUCCESS;
}

/// Get the ID for the specified thread.
static ResultCode GetThreadId(u32* thread_id, Handle handle) {
    LOG_TRACE(Kernel_SVC, "called thread=0x%08X", handle);

    const SharedPtr<Kernel::Thread> thread = Kernel::g_handle_table.Get<Kernel::Thread>(handle);
    if (thread == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    *thread_id = thread->GetThreadId();
    return RESULT_SUCCESS;
}

/// Creates a semaphore
static ResultCode CreateSemaphore(Handle* semaphore, s32 initial_count, s32 max_count) {
    using Kernel::Semaphore;

    ResultVal<SharedPtr<Semaphore>> semaphore_res = Semaphore::Create(initial_count, max_count);
    if (semaphore_res.Failed())
        return semaphore_res.Code();

    ResultVal<Handle> handle_res = Kernel::g_handle_table.Create(*semaphore_res);
    if (handle_res.Failed())
        return handle_res.Code();

    *semaphore = *handle_res;
    LOG_TRACE(Kernel_SVC, "called initial_count=%d, max_count=%d, created handle=0x%08X",
        initial_count, max_count, *semaphore);
    return RESULT_SUCCESS;
}

/// Releases a certain number of slots in a semaphore
static ResultCode ReleaseSemaphore(s32* count, Handle handle, s32 release_count) {
    using Kernel::Semaphore;

    LOG_TRACE(Kernel_SVC, "called release_count=%d, handle=0x%08X", release_count, handle);

    SharedPtr<Semaphore> semaphore = Kernel::g_handle_table.Get<Semaphore>(handle);
    if (semaphore == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    ResultVal<s32> release_res = semaphore->Release(release_count);
    if (release_res.Failed())
        return release_res.Code();

    *count = *release_res;
    return RESULT_SUCCESS;
}

/// Query memory
static ResultCode QueryMemory(void* info, void* out, u32 addr) {
    LOG_ERROR(Kernel_SVC, "(UNIMPLEMENTED) called addr=0x%08X", addr);
    return RESULT_SUCCESS;
}

/// Create an event
static ResultCode CreateEvent(Handle* handle, u32 reset_type) {
    auto evt_res = Kernel::Event::Create(static_cast<ResetType>(reset_type));
    if (evt_res.Failed())
        return evt_res.Code();
    auto handle_res = Kernel::g_handle_table.Create(evt_res.MoveFrom());
    if (handle_res.Failed())
        return handle_res.Code();
    *handle = handle_res.MoveFrom();

    LOG_TRACE(Kernel_SVC, "called reset_type=0x%08X : created handle=0x%08X", reset_type, *handle);
    return RESULT_SUCCESS;
}

/// Duplicates a kernel handle
static ResultCode DuplicateHandle(Handle* out, Handle handle) {
    ResultVal<Handle> out_h = Kernel::g_handle_table.Duplicate(handle);
    if (out_h.Succeeded()) {
        *out = *out_h;
        LOG_TRACE(Kernel_SVC, "duplicated 0x%08X to 0x%08X", handle, *out);
    }
    return out_h.Code();
}

/// Signals an event
static ResultCode SignalEvent(Handle handle) {
    LOG_TRACE(Kernel_SVC, "called event=0x%08X", handle);

    auto evt = Kernel::g_handle_table.Get<Kernel::Event>(handle);
    if (evt == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    evt->Signal();
    HLE::Reschedule(__func__);
    return RESULT_SUCCESS;
}

/// Clears an event
static ResultCode ClearEvent(Handle handle) {
    LOG_TRACE(Kernel_SVC, "called event=0x%08X", handle);

    auto evt = Kernel::g_handle_table.Get<Kernel::Event>(handle);
    if (evt == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    evt->Clear();
    return RESULT_SUCCESS;
}

/// Creates a timer
static ResultCode CreateTimer(Handle* handle, u32 reset_type) {
    using Kernel::Timer;

    auto timer_res = Timer::Create(static_cast<ResetType>(reset_type));
    if (timer_res.Failed())
        return timer_res.Code();

    auto handle_res = Kernel::g_handle_table.Create(timer_res.MoveFrom());
    if (handle_res.Failed())
        return handle_res.Code();
    *handle = handle_res.MoveFrom();

    LOG_TRACE(Kernel_SVC, "called reset_type=0x%08X : created handle=0x%08X", reset_type, *handle);
    return RESULT_SUCCESS;
}

/// Clears a timer
static ResultCode ClearTimer(Handle handle) {
    using Kernel::Timer;

    LOG_TRACE(Kernel_SVC, "called timer=0x%08X", handle);

    SharedPtr<Timer> timer = Kernel::g_handle_table.Get<Timer>(handle);
    if (timer == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    timer->Clear();
    return RESULT_SUCCESS;
}

/// Starts a timer
static ResultCode SetTimer(Handle handle, s64 initial, s64 interval) {
    using Kernel::Timer;

    LOG_TRACE(Kernel_SVC, "called timer=0x%08X", handle);

    SharedPtr<Timer> timer = Kernel::g_handle_table.Get<Timer>(handle);
    if (timer == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    timer->Set(initial, interval);
    return RESULT_SUCCESS;
}

/// Cancels a timer
static ResultCode CancelTimer(Handle handle) {
    using Kernel::Timer;

    LOG_TRACE(Kernel_SVC, "called timer=0x%08X", handle);

    SharedPtr<Timer> timer = Kernel::g_handle_table.Get<Timer>(handle);
    if (timer == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    timer->Cancel();
    return RESULT_SUCCESS;
}

/// Sleep the current thread
static void SleepThread(s64 nanoseconds) {
    LOG_TRACE(Kernel_SVC, "called nanoseconds=%lld", nanoseconds);

    // Sleep current thread and check for next thread to schedule
    Kernel::WaitCurrentThread_Sleep();

    // Create an event to wake the thread up after the specified nanosecond delay has passed
    Kernel::WakeThreadAfterDelay(Kernel::GetCurrentThread(), nanoseconds);

    HLE::Reschedule(__func__);
}

/// This returns the total CPU ticks elapsed since the CPU was powered-on
static s64 GetSystemTick() {
    return (s64)Core::g_app_core->GetTicks();
}

/// Creates a memory block at the specified address with the specified permissions and size
static ResultCode CreateMemoryBlock(Handle* memblock, u32 addr, u32 size, u32 my_permission,
        u32 other_permission) {
    using Kernel::SharedMemory;
    // TODO(Subv): Implement this function

    ResultVal<SharedPtr<SharedMemory>> shared_memory_res = SharedMemory::Create();
    if (shared_memory_res.Failed())
        return shared_memory_res.Code();

    ResultVal<Handle> handle_res = Kernel::g_handle_table.Create(*shared_memory_res);
    if (handle_res.Failed())
        return handle_res.Code();

    *memblock = *handle_res;
    LOG_WARNING(Kernel_SVC, "(STUBBED) called addr=0x%08X", addr);
    return RESULT_SUCCESS;
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
    {0x09, ExitThread,                      "ExitThread"},
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
    {0x1A, HLE::Wrap<CreateTimer>,          "CreateTimer"},
    {0x1B, HLE::Wrap<SetTimer>,             "SetTimer"},
    {0x1C, HLE::Wrap<CancelTimer>,          "CancelTimer"},
    {0x1D, HLE::Wrap<ClearTimer>,           "ClearTimer"},
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
    {0x73, nullptr,                         "CreateCodeSet"},
    {0x74, nullptr,                         "RandomStub"},
    {0x75, nullptr,                         "CreateProcess"},
    {0x76, nullptr,                         "TerminateProcess"},
    {0x77, nullptr,                         "SetProcessResourceLimits"},
    {0x78, nullptr,                         "CreateResourceLimit"},
    {0x79, nullptr,                         "SetResourceLimitValues"},
    {0x7A, nullptr,                         "AddCodeSegment"},
    {0x7B, nullptr,                         "Backdoor"},
    {0x7C, nullptr,                         "KernelSetState"},
    {0x7D, nullptr,                         "QueryProcessMemory"},
};

void Register() {
    HLE::RegisterModule("SVC_Table", ARRAY_SIZE(SVC_Table), SVC_Table);
}

} // namespace
