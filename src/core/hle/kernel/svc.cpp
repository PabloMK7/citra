// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <map>
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "common/string_util.h"
#include "core/arm/arm_interface.h"
#include "core/core_timing.h"
#include "core/hle/function_wrappers.h"
#include "core/hle/kernel/address_arbiter.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/ipc.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/semaphore.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/kernel/thread.h"
#include "core/hle/kernel/timer.h"
#include "core/hle/kernel/vm_manager.h"
#include "core/hle/kernel/wait_object.h"
#include "core/hle/lock.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

namespace Kernel {

enum ControlMemoryOperation {
    MEMOP_FREE = 1,
    MEMOP_RESERVE = 2, // This operation seems to be unsupported in the kernel
    MEMOP_COMMIT = 3,
    MEMOP_MAP = 4,
    MEMOP_UNMAP = 5,
    MEMOP_PROTECT = 6,
    MEMOP_OPERATION_MASK = 0xFF,

    MEMOP_REGION_APP = 0x100,
    MEMOP_REGION_SYSTEM = 0x200,
    MEMOP_REGION_BASE = 0x300,
    MEMOP_REGION_MASK = 0xF00,

    MEMOP_LINEAR = 0x10000,
};

/// Map application or GSP heap memory
static ResultCode ControlMemory(u32* out_addr, u32 operation, u32 addr0, u32 addr1, u32 size,
                                u32 permissions) {
    LOG_DEBUG(Kernel_SVC,
              "called operation=0x%08X, addr0=0x%08X, addr1=0x%08X, size=0x%X, permissions=0x%08X",
              operation, addr0, addr1, size, permissions);

    if ((addr0 & Memory::PAGE_MASK) != 0 || (addr1 & Memory::PAGE_MASK) != 0) {
        return ERR_MISALIGNED_ADDRESS;
    }
    if ((size & Memory::PAGE_MASK) != 0) {
        return ERR_MISALIGNED_SIZE;
    }

    u32 region = operation & MEMOP_REGION_MASK;
    operation &= ~MEMOP_REGION_MASK;

    if (region != 0) {
        LOG_WARNING(Kernel_SVC, "ControlMemory with specified region not supported, region=%X",
                    region);
    }

    if ((permissions & (u32)MemoryPermission::ReadWrite) != permissions) {
        return ERR_INVALID_COMBINATION;
    }
    VMAPermission vma_permissions = (VMAPermission)permissions;

    auto& process = *g_current_process;

    switch (operation & MEMOP_OPERATION_MASK) {
    case MEMOP_FREE: {
        // TODO(Subv): What happens if an application tries to FREE a block of memory that has a
        // SharedMemory pointing to it?
        if (addr0 >= Memory::HEAP_VADDR && addr0 < Memory::HEAP_VADDR_END) {
            ResultCode result = process.HeapFree(addr0, size);
            if (result.IsError())
                return result;
        } else if (addr0 >= process.GetLinearHeapBase() && addr0 < process.GetLinearHeapLimit()) {
            ResultCode result = process.LinearFree(addr0, size);
            if (result.IsError())
                return result;
        } else {
            return ERR_INVALID_ADDRESS;
        }
        *out_addr = addr0;
        break;
    }

    case MEMOP_COMMIT: {
        if (operation & MEMOP_LINEAR) {
            CASCADE_RESULT(*out_addr, process.LinearAllocate(addr0, size, vma_permissions));
        } else {
            CASCADE_RESULT(*out_addr, process.HeapAllocate(addr0, size, vma_permissions));
        }
        break;
    }

    case MEMOP_MAP: {
        // TODO: This is just a hack to avoid regressions until memory aliasing is implemented
        CASCADE_RESULT(*out_addr, process.HeapAllocate(addr0, size, vma_permissions));
        break;
    }

    case MEMOP_UNMAP: {
        // TODO: This is just a hack to avoid regressions until memory aliasing is implemented
        ResultCode result = process.HeapFree(addr0, size);
        if (result.IsError())
            return result;
        break;
    }

    case MEMOP_PROTECT: {
        ResultCode result = process.vm_manager.ReprotectRange(addr0, size, vma_permissions);
        if (result.IsError())
            return result;
        break;
    }

    default:
        LOG_ERROR(Kernel_SVC, "unknown operation=0x%08X", operation);
        return ERR_INVALID_COMBINATION;
    }

    process.vm_manager.LogLayout(Log::Level::Trace);

    return RESULT_SUCCESS;
}

static void ExitProcess() {
    LOG_INFO(Kernel_SVC, "Process %u exiting", g_current_process->process_id);

    ASSERT_MSG(g_current_process->status == ProcessStatus::Running, "Process has already exited");

    g_current_process->status = ProcessStatus::Exited;

    // Stop all the process threads that are currently waiting for objects.
    auto& thread_list = GetThreadList();
    for (auto& thread : thread_list) {
        if (thread->owner_process != g_current_process)
            continue;

        if (thread == GetCurrentThread())
            continue;

        // TODO(Subv): When are the other running/ready threads terminated?
        ASSERT_MSG(thread->status == THREADSTATUS_WAIT_SYNCH_ANY ||
                       thread->status == THREADSTATUS_WAIT_SYNCH_ALL,
                   "Exiting processes with non-waiting threads is currently unimplemented");

        thread->Stop();
    }

    // Kill the current thread
    GetCurrentThread()->Stop();

    Core::System::GetInstance().PrepareReschedule();
}

/// Maps a memory block to specified address
static ResultCode MapMemoryBlock(Handle handle, u32 addr, u32 permissions, u32 other_permissions) {
    LOG_TRACE(Kernel_SVC,
              "called memblock=0x%08X, addr=0x%08X, mypermissions=0x%08X, otherpermission=%d",
              handle, addr, permissions, other_permissions);

    SharedPtr<SharedMemory> shared_memory = g_handle_table.Get<SharedMemory>(handle);
    if (shared_memory == nullptr)
        return ERR_INVALID_HANDLE;

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
        return shared_memory->Map(g_current_process.get(), addr, permissions_type,
                                  static_cast<MemoryPermission>(other_permissions));
    default:
        LOG_ERROR(Kernel_SVC, "unknown permissions=0x%08X", permissions);
    }

    return ERR_INVALID_COMBINATION;
}

static ResultCode UnmapMemoryBlock(Handle handle, u32 addr) {
    LOG_TRACE(Kernel_SVC, "called memblock=0x%08X, addr=0x%08X", handle, addr);

    // TODO(Subv): Return E0A01BF5 if the address is not in the application's heap

    SharedPtr<SharedMemory> shared_memory = g_handle_table.Get<SharedMemory>(handle);
    if (shared_memory == nullptr)
        return ERR_INVALID_HANDLE;

    return shared_memory->Unmap(g_current_process.get(), addr);
}

/// Connect to an OS service given the port name, returns the handle to the port to out
static ResultCode ConnectToPort(Handle* out_handle, VAddr port_name_address) {
    if (!Memory::IsValidVirtualAddress(port_name_address))
        return ERR_NOT_FOUND;

    static constexpr std::size_t PortNameMaxLength = 11;
    // Read 1 char beyond the max allowed port name to detect names that are too long.
    std::string port_name = Memory::ReadCString(port_name_address, PortNameMaxLength + 1);
    if (port_name.size() > PortNameMaxLength)
        return ERR_PORT_NAME_TOO_LONG;

    LOG_TRACE(Kernel_SVC, "called port_name=%s", port_name.c_str());

    auto it = Service::g_kernel_named_ports.find(port_name);
    if (it == Service::g_kernel_named_ports.end()) {
        LOG_WARNING(Kernel_SVC, "tried to connect to unknown port: %s", port_name.c_str());
        return ERR_NOT_FOUND;
    }

    auto client_port = it->second;

    SharedPtr<ClientSession> client_session;
    CASCADE_RESULT(client_session, client_port->Connect());

    // Return the client session
    CASCADE_RESULT(*out_handle, g_handle_table.Create(client_session));
    return RESULT_SUCCESS;
}

/// Makes a blocking IPC call to an OS service.
static ResultCode SendSyncRequest(Handle handle) {
    SharedPtr<ClientSession> session = g_handle_table.Get<ClientSession>(handle);
    if (session == nullptr) {
        return ERR_INVALID_HANDLE;
    }

    LOG_TRACE(Kernel_SVC, "called handle=0x%08X(%s)", handle, session->GetName().c_str());

    Core::System::GetInstance().PrepareReschedule();

    return session->SendSyncRequest(GetCurrentThread());
}

/// Close a handle
static ResultCode CloseHandle(Handle handle) {
    LOG_TRACE(Kernel_SVC, "Closing handle 0x%08X", handle);
    return g_handle_table.Close(handle);
}

/// Wait for a handle to synchronize, timeout after the specified nanoseconds
static ResultCode WaitSynchronization1(Handle handle, s64 nano_seconds) {
    auto object = g_handle_table.Get<WaitObject>(handle);
    Thread* thread = GetCurrentThread();

    if (object == nullptr)
        return ERR_INVALID_HANDLE;

    LOG_TRACE(Kernel_SVC, "called handle=0x%08X(%s:%s), nanoseconds=%lld", handle,
              object->GetTypeName().c_str(), object->GetName().c_str(), nano_seconds);

    if (object->ShouldWait(thread)) {

        if (nano_seconds == 0)
            return RESULT_TIMEOUT;

        thread->wait_objects = {object};
        object->AddWaitingThread(thread);
        thread->status = THREADSTATUS_WAIT_SYNCH_ANY;

        // Create an event to wake the thread up after the specified nanosecond delay has passed
        thread->WakeAfterDelay(nano_seconds);

        thread->wakeup_callback = [](ThreadWakeupReason reason, SharedPtr<Thread> thread,
                                     SharedPtr<WaitObject> object) {

            ASSERT(thread->status == THREADSTATUS_WAIT_SYNCH_ANY);

            if (reason == ThreadWakeupReason::Timeout) {
                thread->SetWaitSynchronizationResult(RESULT_TIMEOUT);
                return;
            }

            ASSERT(reason == ThreadWakeupReason::Signal);
            thread->SetWaitSynchronizationResult(RESULT_SUCCESS);

            // WaitSynchronization1 doesn't have an output index like WaitSynchronizationN, so we
            // don't have to do anything else here.
        };

        Core::System::GetInstance().PrepareReschedule();

        // Note: The output of this SVC will be set to RESULT_SUCCESS if the thread
        // resumes due to a signal in its wait objects.
        // Otherwise we retain the default value of timeout.
        return RESULT_TIMEOUT;
    }

    object->Acquire(thread);

    return RESULT_SUCCESS;
}

/// Wait for the given handles to synchronize, timeout after the specified nanoseconds
static ResultCode WaitSynchronizationN(s32* out, VAddr handles_address, s32 handle_count,
                                       bool wait_all, s64 nano_seconds) {
    Thread* thread = GetCurrentThread();

    if (!Memory::IsValidVirtualAddress(handles_address))
        return ERR_INVALID_POINTER;

    // NOTE: on real hardware, there is no nullptr check for 'out' (tested with firmware 4.4). If
    // this happens, the running application will crash.
    ASSERT_MSG(out != nullptr, "invalid output pointer specified!");

    // Check if 'handle_count' is invalid
    if (handle_count < 0)
        return ERR_OUT_OF_RANGE;

    using ObjectPtr = SharedPtr<WaitObject>;
    std::vector<ObjectPtr> objects(handle_count);

    for (int i = 0; i < handle_count; ++i) {
        Handle handle = Memory::Read32(handles_address + i * sizeof(Handle));
        auto object = g_handle_table.Get<WaitObject>(handle);
        if (object == nullptr)
            return ERR_INVALID_HANDLE;
        objects[i] = object;
    }

    if (wait_all) {
        bool all_available =
            std::all_of(objects.begin(), objects.end(),
                        [thread](const ObjectPtr& object) { return !object->ShouldWait(thread); });
        if (all_available) {
            // We can acquire all objects right now, do so.
            for (auto& object : objects)
                object->Acquire(thread);
            // Note: In this case, the `out` parameter is not set,
            // and retains whatever value it had before.
            return RESULT_SUCCESS;
        }

        // Not all objects were available right now, prepare to suspend the thread.

        // If a timeout value of 0 was provided, just return the Timeout error code instead of
        // suspending the thread.
        if (nano_seconds == 0)
            return RESULT_TIMEOUT;

        // Put the thread to sleep
        thread->status = THREADSTATUS_WAIT_SYNCH_ALL;

        // Add the thread to each of the objects' waiting threads.
        for (auto& object : objects) {
            object->AddWaitingThread(thread);
        }

        thread->wait_objects = std::move(objects);

        // Create an event to wake the thread up after the specified nanosecond delay has passed
        thread->WakeAfterDelay(nano_seconds);

        thread->wakeup_callback = [](ThreadWakeupReason reason, SharedPtr<Thread> thread,
                                     SharedPtr<WaitObject> object) {

            ASSERT(thread->status == THREADSTATUS_WAIT_SYNCH_ALL);

            if (reason == ThreadWakeupReason::Timeout) {
                thread->SetWaitSynchronizationResult(RESULT_TIMEOUT);
                return;
            }

            ASSERT(reason == ThreadWakeupReason::Signal);

            thread->SetWaitSynchronizationResult(RESULT_SUCCESS);
            // The wait_all case does not update the output index.
        };

        Core::System::GetInstance().PrepareReschedule();

        // This value gets set to -1 by default in this case, it is not modified after this.
        *out = -1;
        // Note: The output of this SVC will be set to RESULT_SUCCESS if the thread resumes due to
        // a signal in one of its wait objects.
        return RESULT_TIMEOUT;
    } else {
        // Find the first object that is acquirable in the provided list of objects
        auto itr = std::find_if(objects.begin(), objects.end(), [thread](const ObjectPtr& object) {
            return !object->ShouldWait(thread);
        });

        if (itr != objects.end()) {
            // We found a ready object, acquire it and set the result value
            WaitObject* object = itr->get();
            object->Acquire(thread);
            *out = static_cast<s32>(std::distance(objects.begin(), itr));
            return RESULT_SUCCESS;
        }

        // No objects were ready to be acquired, prepare to suspend the thread.

        // If a timeout value of 0 was provided, just return the Timeout error code instead of
        // suspending the thread.
        if (nano_seconds == 0)
            return RESULT_TIMEOUT;

        // Put the thread to sleep
        thread->status = THREADSTATUS_WAIT_SYNCH_ANY;

        // Add the thread to each of the objects' waiting threads.
        for (size_t i = 0; i < objects.size(); ++i) {
            WaitObject* object = objects[i].get();
            object->AddWaitingThread(thread);
        }

        thread->wait_objects = std::move(objects);

        // Note: If no handles and no timeout were given, then the thread will deadlock, this is
        // consistent with hardware behavior.

        // Create an event to wake the thread up after the specified nanosecond delay has passed
        thread->WakeAfterDelay(nano_seconds);

        thread->wakeup_callback = [](ThreadWakeupReason reason, SharedPtr<Thread> thread,
                                     SharedPtr<WaitObject> object) {

            ASSERT(thread->status == THREADSTATUS_WAIT_SYNCH_ANY);

            if (reason == ThreadWakeupReason::Timeout) {
                thread->SetWaitSynchronizationResult(RESULT_TIMEOUT);
                return;
            }

            ASSERT(reason == ThreadWakeupReason::Signal);

            thread->SetWaitSynchronizationResult(RESULT_SUCCESS);
            thread->SetWaitSynchronizationOutput(thread->GetWaitObjectIndex(object.get()));
        };

        Core::System::GetInstance().PrepareReschedule();

        // Note: The output of this SVC will be set to RESULT_SUCCESS if the thread resumes due to a
        // signal in one of its wait objects.
        // Otherwise we retain the default value of timeout, and -1 in the out parameter
        *out = -1;
        return RESULT_TIMEOUT;
    }
}

static ResultCode ReceiveIPCRequest(SharedPtr<ServerSession> server_session,
                                    SharedPtr<Thread> thread) {
    if (server_session->parent->client == nullptr) {
        return ERR_SESSION_CLOSED_BY_REMOTE;
    }

    VAddr target_address = thread->GetCommandBufferAddress();
    VAddr source_address = server_session->currently_handling->GetCommandBufferAddress();

    ResultCode translation_result = TranslateCommandBuffer(
        server_session->currently_handling, thread, source_address, target_address, false);

    // If a translation error occurred, immediately resume the client thread.
    if (translation_result.IsError()) {
        // Set the output of SendSyncRequest in the client thread to the translation output.
        server_session->currently_handling->SetWaitSynchronizationResult(translation_result);

        server_session->currently_handling->ResumeFromWait();
        server_session->currently_handling = nullptr;

        // TODO(Subv): This path should try to wait again on the same objects.
        ASSERT_MSG(false, "ReplyAndReceive translation error behavior unimplemented");
    }

    return translation_result;
}

/// In a single operation, sends a IPC reply and waits for a new request.
static ResultCode ReplyAndReceive(s32* index, VAddr handles_address, s32 handle_count,
                                  Handle reply_target) {
    if (!Memory::IsValidVirtualAddress(handles_address))
        return ERR_INVALID_POINTER;

    // Check if 'handle_count' is invalid
    if (handle_count < 0)
        return ERR_OUT_OF_RANGE;

    using ObjectPtr = SharedPtr<WaitObject>;
    std::vector<ObjectPtr> objects(handle_count);

    for (int i = 0; i < handle_count; ++i) {
        Handle handle = Memory::Read32(handles_address + i * sizeof(Handle));
        auto object = g_handle_table.Get<WaitObject>(handle);
        if (object == nullptr)
            return ERR_INVALID_HANDLE;
        objects[i] = object;
    }

    // We are also sending a command reply.
    // Do not send a reply if the command id in the command buffer is 0xFFFF.
    u32* cmd_buff = GetCommandBuffer();
    IPC::Header header{cmd_buff[0]};
    if (reply_target != 0 && header.command_id != 0xFFFF) {
        auto session = g_handle_table.Get<ServerSession>(reply_target);
        if (session == nullptr)
            return ERR_INVALID_HANDLE;

        auto request_thread = std::move(session->currently_handling);

        // Mark the request as "handled".
        session->currently_handling = nullptr;

        // Error out if there's no request thread or the session was closed.
        // TODO(Subv): Is the same error code (ClosedByRemote) returned for both of these cases?
        if (request_thread == nullptr || session->parent->client == nullptr) {
            *index = -1;
            return ERR_SESSION_CLOSED_BY_REMOTE;
        }

        VAddr source_address = GetCurrentThread()->GetCommandBufferAddress();
        VAddr target_address = request_thread->GetCommandBufferAddress();

        ResultCode translation_result = TranslateCommandBuffer(
            Kernel::GetCurrentThread(), request_thread, source_address, target_address, true);

        // Note: The real kernel seems to always panic if the Server->Client buffer translation
        // fails for whatever reason.
        ASSERT(translation_result.IsSuccess());

        // Note: The scheduler is not invoked here.
        request_thread->ResumeFromWait();
    }

    if (handle_count == 0) {
        *index = 0;
        // The kernel uses this value as a placeholder for the real error, and returns it when we
        // pass no handles and do not perform any reply.
        if (reply_target == 0 || header.command_id == 0xFFFF)
            return ResultCode(0xE7E3FFFF);

        return RESULT_SUCCESS;
    }

    auto thread = GetCurrentThread();

    // Find the first object that is acquirable in the provided list of objects
    auto itr = std::find_if(objects.begin(), objects.end(), [thread](const ObjectPtr& object) {
        return !object->ShouldWait(thread);
    });

    if (itr != objects.end()) {
        // We found a ready object, acquire it and set the result value
        WaitObject* object = itr->get();
        object->Acquire(thread);
        *index = static_cast<s32>(std::distance(objects.begin(), itr));

        if (object->GetHandleType() != HandleType::ServerSession)
            return RESULT_SUCCESS;

        auto server_session = static_cast<ServerSession*>(object);
        return ReceiveIPCRequest(server_session, GetCurrentThread());
    }

    // No objects were ready to be acquired, prepare to suspend the thread.

    // Put the thread to sleep
    thread->status = THREADSTATUS_WAIT_SYNCH_ANY;

    // Add the thread to each of the objects' waiting threads.
    for (size_t i = 0; i < objects.size(); ++i) {
        WaitObject* object = objects[i].get();
        object->AddWaitingThread(thread);
    }

    thread->wait_objects = std::move(objects);

    thread->wakeup_callback = [](ThreadWakeupReason reason, SharedPtr<Thread> thread,
                                 SharedPtr<WaitObject> object) {

        ASSERT(thread->status == THREADSTATUS_WAIT_SYNCH_ANY);
        ASSERT(reason == ThreadWakeupReason::Signal);

        ResultCode result = RESULT_SUCCESS;

        if (object->GetHandleType() == HandleType::ServerSession) {
            auto server_session = DynamicObjectCast<ServerSession>(object);
            result = ReceiveIPCRequest(server_session, thread);
        }

        thread->SetWaitSynchronizationResult(result);
        thread->SetWaitSynchronizationOutput(thread->GetWaitObjectIndex(object.get()));
    };

    Core::System::GetInstance().PrepareReschedule();

    // Note: The output of this SVC will be set to RESULT_SUCCESS if the thread resumes due to a
    // signal in one of its wait objects, or to 0xC8A01836 if there was a translation error.
    // By default the index is set to -1.
    *index = -1;
    return RESULT_SUCCESS;
}

/// Create an address arbiter (to allocate access to shared resources)
static ResultCode CreateAddressArbiter(Handle* out_handle) {
    SharedPtr<AddressArbiter> arbiter = AddressArbiter::Create();
    CASCADE_RESULT(*out_handle, g_handle_table.Create(std::move(arbiter)));
    LOG_TRACE(Kernel_SVC, "returned handle=0x%08X", *out_handle);
    return RESULT_SUCCESS;
}

/// Arbitrate address
static ResultCode ArbitrateAddress(Handle handle, u32 address, u32 type, u32 value,
                                   s64 nanoseconds) {
    LOG_TRACE(Kernel_SVC, "called handle=0x%08X, address=0x%08X, type=0x%08X, value=0x%08X", handle,
              address, type, value);

    SharedPtr<AddressArbiter> arbiter = g_handle_table.Get<AddressArbiter>(handle);
    if (arbiter == nullptr)
        return ERR_INVALID_HANDLE;

    auto res = arbiter->ArbitrateAddress(GetCurrentThread(), static_cast<ArbitrationType>(type),
                                         address, value, nanoseconds);

    // TODO(Subv): Identify in which specific cases this call should cause a reschedule.
    Core::System::GetInstance().PrepareReschedule();

    return res;
}

static void Break(u8 break_reason) {
    LOG_CRITICAL(Debug_Emulated, "Emulated program broke execution!");
    std::string reason_str;
    switch (break_reason) {
    case 0:
        reason_str = "PANIC";
        break;
    case 1:
        reason_str = "ASSERT";
        break;
    case 2:
        reason_str = "USER";
        break;
    default:
        reason_str = "UNKNOWN";
        break;
    }
    LOG_CRITICAL(Debug_Emulated, "Break reason: %s", reason_str.c_str());
}

/// Used to output a message on a debug hardware unit - does nothing on a retail unit
static void OutputDebugString(VAddr address, int len) {
    std::vector<char> string(len);
    Memory::ReadBlock(address, string.data(), len);
    LOG_DEBUG(Debug_Emulated, "%.*s", len, string.data());
}

/// Get resource limit
static ResultCode GetResourceLimit(Handle* resource_limit, Handle process_handle) {
    LOG_TRACE(Kernel_SVC, "called process=0x%08X", process_handle);

    SharedPtr<Process> process = g_handle_table.Get<Process>(process_handle);
    if (process == nullptr)
        return ERR_INVALID_HANDLE;

    CASCADE_RESULT(*resource_limit, g_handle_table.Create(process->resource_limit));

    return RESULT_SUCCESS;
}

/// Get resource limit current values
static ResultCode GetResourceLimitCurrentValues(VAddr values, Handle resource_limit_handle,
                                                VAddr names, u32 name_count) {
    LOG_TRACE(Kernel_SVC, "called resource_limit=%08X, names=%08X, name_count=%d",
              resource_limit_handle, names, name_count);

    SharedPtr<ResourceLimit> resource_limit =
        g_handle_table.Get<ResourceLimit>(resource_limit_handle);
    if (resource_limit == nullptr)
        return ERR_INVALID_HANDLE;

    for (unsigned int i = 0; i < name_count; ++i) {
        u32 name = Memory::Read32(names + i * sizeof(u32));
        s64 value = resource_limit->GetCurrentResourceValue(name);
        Memory::Write64(values + i * sizeof(u64), value);
    }

    return RESULT_SUCCESS;
}

/// Get resource limit max values
static ResultCode GetResourceLimitLimitValues(VAddr values, Handle resource_limit_handle,
                                              VAddr names, u32 name_count) {
    LOG_TRACE(Kernel_SVC, "called resource_limit=%08X, names=%08X, name_count=%d",
              resource_limit_handle, names, name_count);

    SharedPtr<ResourceLimit> resource_limit =
        g_handle_table.Get<ResourceLimit>(resource_limit_handle);
    if (resource_limit == nullptr)
        return ERR_INVALID_HANDLE;

    for (unsigned int i = 0; i < name_count; ++i) {
        u32 name = Memory::Read32(names + i * sizeof(u32));
        s64 value = resource_limit->GetMaxResourceValue(name);
        Memory::Write64(values + i * sizeof(u64), value);
    }

    return RESULT_SUCCESS;
}

/// Creates a new thread
static ResultCode CreateThread(Handle* out_handle, u32 priority, u32 entry_point, u32 arg,
                               u32 stack_top, s32 processor_id) {
    std::string name = Common::StringFromFormat("unknown-%08" PRIX32, entry_point);

    if (priority > THREADPRIO_LOWEST) {
        return ERR_OUT_OF_RANGE;
    }

    SharedPtr<ResourceLimit>& resource_limit = g_current_process->resource_limit;
    if (resource_limit->GetMaxResourceValue(ResourceTypes::PRIORITY) > priority) {
        return ERR_NOT_AUTHORIZED;
    }

    if (processor_id == THREADPROCESSORID_DEFAULT) {
        // Set the target CPU to the one specified in the process' exheader.
        processor_id = g_current_process->ideal_processor;
        ASSERT(processor_id != THREADPROCESSORID_DEFAULT);
    }

    switch (processor_id) {
    case THREADPROCESSORID_0:
        break;
    case THREADPROCESSORID_ALL:
        LOG_INFO(Kernel_SVC,
                 "Newly created thread is allowed to be run in any Core, unimplemented.");
        break;
    case THREADPROCESSORID_1:
        LOG_ERROR(Kernel_SVC,
                  "Newly created thread must run in the SysCore (Core1), unimplemented.");
        break;
    default:
        // TODO(bunnei): Implement support for other processor IDs
        ASSERT_MSG(false, "Unsupported thread processor ID: %d", processor_id);
        break;
    }

    CASCADE_RESULT(SharedPtr<Thread> thread,
                   Thread::Create(name, entry_point, priority, arg, processor_id, stack_top,
                                  g_current_process));

    thread->context->SetFpscr(FPSCR_DEFAULT_NAN | FPSCR_FLUSH_TO_ZERO |
                              FPSCR_ROUND_TOZERO); // 0x03C00000

    CASCADE_RESULT(*out_handle, g_handle_table.Create(std::move(thread)));

    Core::System::GetInstance().PrepareReschedule();

    LOG_TRACE(Kernel_SVC, "called entrypoint=0x%08X (%s), arg=0x%08X, stacktop=0x%08X, "
                          "threadpriority=0x%08X, processorid=0x%08X : created handle=0x%08X",
              entry_point, name.c_str(), arg, stack_top, priority, processor_id, *out_handle);

    return RESULT_SUCCESS;
}

/// Called when a thread exits
static void ExitThread() {
    LOG_TRACE(Kernel_SVC, "called, pc=0x%08X", Core::CPU().GetPC());

    ExitCurrentThread();
    Core::System::GetInstance().PrepareReschedule();
}

/// Gets the priority for the specified thread
static ResultCode GetThreadPriority(u32* priority, Handle handle) {
    const SharedPtr<Thread> thread = g_handle_table.Get<Thread>(handle);
    if (thread == nullptr)
        return ERR_INVALID_HANDLE;

    *priority = thread->GetPriority();
    return RESULT_SUCCESS;
}

/// Sets the priority for the specified thread
static ResultCode SetThreadPriority(Handle handle, u32 priority) {
    if (priority > THREADPRIO_LOWEST) {
        return ERR_OUT_OF_RANGE;
    }

    SharedPtr<Thread> thread = g_handle_table.Get<Thread>(handle);
    if (thread == nullptr)
        return ERR_INVALID_HANDLE;

    // Note: The kernel uses the current process's resource limit instead of
    // the one from the thread owner's resource limit.
    SharedPtr<ResourceLimit>& resource_limit = g_current_process->resource_limit;
    if (resource_limit->GetMaxResourceValue(ResourceTypes::PRIORITY) > priority) {
        return ERR_NOT_AUTHORIZED;
    }

    thread->SetPriority(priority);
    thread->UpdatePriority();

    // Update the mutexes that this thread is waiting for
    for (auto& mutex : thread->pending_mutexes)
        mutex->UpdatePriority();

    Core::System::GetInstance().PrepareReschedule();
    return RESULT_SUCCESS;
}

/// Create a mutex
static ResultCode CreateMutex(Handle* out_handle, u32 initial_locked) {
    SharedPtr<Mutex> mutex = Mutex::Create(initial_locked != 0);
    mutex->name = Common::StringFromFormat("mutex-%08x", Core::CPU().GetReg(14));
    CASCADE_RESULT(*out_handle, g_handle_table.Create(std::move(mutex)));

    LOG_TRACE(Kernel_SVC, "called initial_locked=%s : created handle=0x%08X",
              initial_locked ? "true" : "false", *out_handle);

    return RESULT_SUCCESS;
}

/// Release a mutex
static ResultCode ReleaseMutex(Handle handle) {
    LOG_TRACE(Kernel_SVC, "called handle=0x%08X", handle);

    SharedPtr<Mutex> mutex = g_handle_table.Get<Mutex>(handle);
    if (mutex == nullptr)
        return ERR_INVALID_HANDLE;

    return mutex->Release(GetCurrentThread());
}

/// Get the ID of the specified process
static ResultCode GetProcessId(u32* process_id, Handle process_handle) {
    LOG_TRACE(Kernel_SVC, "called process=0x%08X", process_handle);

    const SharedPtr<Process> process = g_handle_table.Get<Process>(process_handle);
    if (process == nullptr)
        return ERR_INVALID_HANDLE;

    *process_id = process->process_id;
    return RESULT_SUCCESS;
}

/// Get the ID of the process that owns the specified thread
static ResultCode GetProcessIdOfThread(u32* process_id, Handle thread_handle) {
    LOG_TRACE(Kernel_SVC, "called thread=0x%08X", thread_handle);

    const SharedPtr<Thread> thread = g_handle_table.Get<Thread>(thread_handle);
    if (thread == nullptr)
        return ERR_INVALID_HANDLE;

    const SharedPtr<Process> process = thread->owner_process;

    ASSERT_MSG(process != nullptr, "Invalid parent process for thread=0x%08X", thread_handle);

    *process_id = process->process_id;
    return RESULT_SUCCESS;
}

/// Get the ID for the specified thread.
static ResultCode GetThreadId(u32* thread_id, Handle handle) {
    LOG_TRACE(Kernel_SVC, "called thread=0x%08X", handle);

    const SharedPtr<Thread> thread = g_handle_table.Get<Thread>(handle);
    if (thread == nullptr)
        return ERR_INVALID_HANDLE;

    *thread_id = thread->GetThreadId();
    return RESULT_SUCCESS;
}

/// Creates a semaphore
static ResultCode CreateSemaphore(Handle* out_handle, s32 initial_count, s32 max_count) {
    CASCADE_RESULT(SharedPtr<Semaphore> semaphore, Semaphore::Create(initial_count, max_count));
    semaphore->name = Common::StringFromFormat("semaphore-%08x", Core::CPU().GetReg(14));
    CASCADE_RESULT(*out_handle, g_handle_table.Create(std::move(semaphore)));

    LOG_TRACE(Kernel_SVC, "called initial_count=%d, max_count=%d, created handle=0x%08X",
              initial_count, max_count, *out_handle);
    return RESULT_SUCCESS;
}

/// Releases a certain number of slots in a semaphore
static ResultCode ReleaseSemaphore(s32* count, Handle handle, s32 release_count) {
    LOG_TRACE(Kernel_SVC, "called release_count=%d, handle=0x%08X", release_count, handle);

    SharedPtr<Semaphore> semaphore = g_handle_table.Get<Semaphore>(handle);
    if (semaphore == nullptr)
        return ERR_INVALID_HANDLE;

    CASCADE_RESULT(*count, semaphore->Release(release_count));

    return RESULT_SUCCESS;
}

/// Query process memory
static ResultCode QueryProcessMemory(MemoryInfo* memory_info, PageInfo* page_info,
                                     Handle process_handle, u32 addr) {
    SharedPtr<Process> process = g_handle_table.Get<Process>(process_handle);
    if (process == nullptr)
        return ERR_INVALID_HANDLE;

    auto vma = process->vm_manager.FindVMA(addr);

    if (vma == g_current_process->vm_manager.vma_map.end())
        return ERR_INVALID_ADDRESS;

    memory_info->base_address = vma->second.base;
    memory_info->permission = static_cast<u32>(vma->second.permissions);
    memory_info->size = vma->second.size;
    memory_info->state = static_cast<u32>(vma->second.meminfo_state);

    page_info->flags = 0;
    LOG_TRACE(Kernel_SVC, "called process=0x%08X addr=0x%08X", process_handle, addr);
    return RESULT_SUCCESS;
}

/// Query memory
static ResultCode QueryMemory(MemoryInfo* memory_info, PageInfo* page_info, u32 addr) {
    return QueryProcessMemory(memory_info, page_info, CurrentProcess, addr);
}

/// Create an event
static ResultCode CreateEvent(Handle* out_handle, u32 reset_type) {
    SharedPtr<Event> evt = Event::Create(static_cast<ResetType>(reset_type));
    evt->name = Common::StringFromFormat("event-%08x", Core::CPU().GetReg(14));
    CASCADE_RESULT(*out_handle, g_handle_table.Create(std::move(evt)));

    LOG_TRACE(Kernel_SVC, "called reset_type=0x%08X : created handle=0x%08X", reset_type,
              *out_handle);
    return RESULT_SUCCESS;
}

/// Duplicates a kernel handle
static ResultCode DuplicateHandle(Handle* out, Handle handle) {
    CASCADE_RESULT(*out, g_handle_table.Duplicate(handle));
    LOG_TRACE(Kernel_SVC, "duplicated 0x%08X to 0x%08X", handle, *out);
    return RESULT_SUCCESS;
}

/// Signals an event
static ResultCode SignalEvent(Handle handle) {
    LOG_TRACE(Kernel_SVC, "called event=0x%08X", handle);

    SharedPtr<Event> evt = g_handle_table.Get<Event>(handle);
    if (evt == nullptr)
        return ERR_INVALID_HANDLE;

    evt->Signal();

    return RESULT_SUCCESS;
}

/// Clears an event
static ResultCode ClearEvent(Handle handle) {
    LOG_TRACE(Kernel_SVC, "called event=0x%08X", handle);

    SharedPtr<Event> evt = g_handle_table.Get<Event>(handle);
    if (evt == nullptr)
        return ERR_INVALID_HANDLE;

    evt->Clear();
    return RESULT_SUCCESS;
}

/// Creates a timer
static ResultCode CreateTimer(Handle* out_handle, u32 reset_type) {
    SharedPtr<Timer> timer = Timer::Create(static_cast<ResetType>(reset_type));
    timer->name = Common::StringFromFormat("timer-%08x", Core::CPU().GetReg(14));
    CASCADE_RESULT(*out_handle, g_handle_table.Create(std::move(timer)));

    LOG_TRACE(Kernel_SVC, "called reset_type=0x%08X : created handle=0x%08X", reset_type,
              *out_handle);
    return RESULT_SUCCESS;
}

/// Clears a timer
static ResultCode ClearTimer(Handle handle) {
    LOG_TRACE(Kernel_SVC, "called timer=0x%08X", handle);

    SharedPtr<Timer> timer = g_handle_table.Get<Timer>(handle);
    if (timer == nullptr)
        return ERR_INVALID_HANDLE;

    timer->Clear();
    return RESULT_SUCCESS;
}

/// Starts a timer
static ResultCode SetTimer(Handle handle, s64 initial, s64 interval) {
    LOG_TRACE(Kernel_SVC, "called timer=0x%08X", handle);

    if (initial < 0 || interval < 0) {
        return ERR_OUT_OF_RANGE_KERNEL;
    }

    SharedPtr<Timer> timer = g_handle_table.Get<Timer>(handle);
    if (timer == nullptr)
        return ERR_INVALID_HANDLE;

    timer->Set(initial, interval);

    return RESULT_SUCCESS;
}

/// Cancels a timer
static ResultCode CancelTimer(Handle handle) {
    LOG_TRACE(Kernel_SVC, "called timer=0x%08X", handle);

    SharedPtr<Timer> timer = g_handle_table.Get<Timer>(handle);
    if (timer == nullptr)
        return ERR_INVALID_HANDLE;

    timer->Cancel();

    return RESULT_SUCCESS;
}

/// Sleep the current thread
static void SleepThread(s64 nanoseconds) {
    LOG_TRACE(Kernel_SVC, "called nanoseconds=%lld", nanoseconds);

    // Don't attempt to yield execution if there are no available threads to run,
    // this way we avoid a useless reschedule to the idle thread.
    if (nanoseconds == 0 && !HaveReadyThreads())
        return;

    // Sleep current thread and check for next thread to schedule
    WaitCurrentThread_Sleep();

    // Create an event to wake the thread up after the specified nanosecond delay has passed
    GetCurrentThread()->WakeAfterDelay(nanoseconds);

    Core::System::GetInstance().PrepareReschedule();
}

/// This returns the total CPU ticks elapsed since the CPU was powered-on
static s64 GetSystemTick() {
    s64 result = CoreTiming::GetTicks();
    // Advance time to defeat dumb games (like Cubic Ninja) that busy-wait for the frame to end.
    CoreTiming::AddTicks(150); // Measured time between two calls on a 9.2 o3DS with Ninjhax 1.1b
    return result;
}

/// Creates a memory block at the specified address with the specified permissions and size
static ResultCode CreateMemoryBlock(Handle* out_handle, u32 addr, u32 size, u32 my_permission,
                                    u32 other_permission) {
    if (size % Memory::PAGE_SIZE != 0)
        return ERR_MISALIGNED_SIZE;

    SharedPtr<SharedMemory> shared_memory = nullptr;

    auto VerifyPermissions = [](MemoryPermission permission) {
        // SharedMemory blocks can not be created with Execute permissions
        switch (permission) {
        case MemoryPermission::None:
        case MemoryPermission::Read:
        case MemoryPermission::Write:
        case MemoryPermission::ReadWrite:
        case MemoryPermission::DontCare:
            return true;
        default:
            return false;
        }
    };

    if (!VerifyPermissions(static_cast<MemoryPermission>(my_permission)) ||
        !VerifyPermissions(static_cast<MemoryPermission>(other_permission)))
        return ERR_INVALID_COMBINATION;

    // TODO(Subv): Processes with memory type APPLICATION are not allowed
    // to create memory blocks with addr = 0, any attempts to do so
    // should return error 0xD92007EA.
    if ((addr < Memory::PROCESS_IMAGE_VADDR || addr + size > Memory::SHARED_MEMORY_VADDR_END) &&
        addr != 0) {
        return ERR_INVALID_ADDRESS;
    }

    // When trying to create a memory block with address = 0,
    // if the process has the Shared Device Memory flag in the exheader,
    // then we have to allocate from the same region as the caller process instead of the BASE
    // region.
    MemoryRegion region = MemoryRegion::BASE;
    if (addr == 0 && g_current_process->flags.shared_device_mem)
        region = g_current_process->flags.memory_region;

    shared_memory =
        SharedMemory::Create(g_current_process, size, static_cast<MemoryPermission>(my_permission),
                             static_cast<MemoryPermission>(other_permission), addr, region);
    CASCADE_RESULT(*out_handle, g_handle_table.Create(std::move(shared_memory)));

    LOG_WARNING(Kernel_SVC, "called addr=0x%08X", addr);
    return RESULT_SUCCESS;
}

static ResultCode CreatePort(Handle* server_port, Handle* client_port, VAddr name_address,
                             u32 max_sessions) {
    // TODO(Subv): Implement named ports.
    ASSERT_MSG(name_address == 0, "Named ports are currently unimplemented");

    auto ports = ServerPort::CreatePortPair(max_sessions);
    CASCADE_RESULT(*client_port,
                   g_handle_table.Create(std::move(std::get<SharedPtr<ClientPort>>(ports))));
    // Note: The 3DS kernel also leaks the client port handle if the server port handle fails to be
    // created.
    CASCADE_RESULT(*server_port,
                   g_handle_table.Create(std::move(std::get<SharedPtr<ServerPort>>(ports))));

    LOG_TRACE(Kernel_SVC, "called max_sessions=%u", max_sessions);
    return RESULT_SUCCESS;
}

static ResultCode CreateSessionToPort(Handle* out_client_session, Handle client_port_handle) {
    SharedPtr<ClientPort> client_port = g_handle_table.Get<ClientPort>(client_port_handle);
    if (client_port == nullptr)
        return ERR_INVALID_HANDLE;

    CASCADE_RESULT(auto session, client_port->Connect());
    CASCADE_RESULT(*out_client_session, g_handle_table.Create(std::move(session)));
    return RESULT_SUCCESS;
}

static ResultCode CreateSession(Handle* server_session, Handle* client_session) {
    auto sessions = ServerSession::CreateSessionPair();

    auto& server = std::get<SharedPtr<ServerSession>>(sessions);
    CASCADE_RESULT(*server_session, g_handle_table.Create(std::move(server)));

    auto& client = std::get<SharedPtr<ClientSession>>(sessions);
    CASCADE_RESULT(*client_session, g_handle_table.Create(std::move(client)));

    LOG_TRACE(Kernel_SVC, "called");
    return RESULT_SUCCESS;
}

static ResultCode AcceptSession(Handle* out_server_session, Handle server_port_handle) {
    SharedPtr<ServerPort> server_port = g_handle_table.Get<ServerPort>(server_port_handle);
    if (server_port == nullptr)
        return ERR_INVALID_HANDLE;

    CASCADE_RESULT(auto session, server_port->Accept());
    CASCADE_RESULT(*out_server_session, g_handle_table.Create(std::move(session)));
    return RESULT_SUCCESS;
}

static ResultCode GetSystemInfo(s64* out, u32 type, s32 param) {
    LOG_TRACE(Kernel_SVC, "called type=%u param=%d", type, param);

    switch ((SystemInfoType)type) {
    case SystemInfoType::REGION_MEMORY_USAGE:
        switch ((SystemInfoMemUsageRegion)param) {
        case SystemInfoMemUsageRegion::ALL:
            *out = GetMemoryRegion(MemoryRegion::APPLICATION)->used +
                   GetMemoryRegion(MemoryRegion::SYSTEM)->used +
                   GetMemoryRegion(MemoryRegion::BASE)->used;
            break;
        case SystemInfoMemUsageRegion::APPLICATION:
            *out = GetMemoryRegion(MemoryRegion::APPLICATION)->used;
            break;
        case SystemInfoMemUsageRegion::SYSTEM:
            *out = GetMemoryRegion(MemoryRegion::SYSTEM)->used;
            break;
        case SystemInfoMemUsageRegion::BASE:
            *out = GetMemoryRegion(MemoryRegion::BASE)->used;
            break;
        default:
            LOG_ERROR(Kernel_SVC, "unknown GetSystemInfo type=0 region: param=%d", param);
            *out = 0;
            break;
        }
        break;
    case SystemInfoType::KERNEL_ALLOCATED_PAGES:
        LOG_ERROR(Kernel_SVC, "unimplemented GetSystemInfo type=2 param=%d", param);
        *out = 0;
        break;
    case SystemInfoType::KERNEL_SPAWNED_PIDS:
        *out = 5;
        break;
    default:
        LOG_ERROR(Kernel_SVC, "unknown GetSystemInfo type=%u param=%d", type, param);
        *out = 0;
        break;
    }

    // This function never returns an error, even if invalid parameters were passed.
    return RESULT_SUCCESS;
}

static ResultCode GetProcessInfo(s64* out, Handle process_handle, u32 type) {
    LOG_TRACE(Kernel_SVC, "called process=0x%08X type=%u", process_handle, type);

    SharedPtr<Process> process = g_handle_table.Get<Process>(process_handle);
    if (process == nullptr)
        return ERR_INVALID_HANDLE;

    switch (type) {
    case 0:
    case 2:
        // TODO(yuriks): Type 0 returns a slightly higher number than type 2, but I'm not sure
        // what's the difference between them.
        *out = process->heap_used + process->linear_heap_used + process->misc_memory_used;
        if (*out % Memory::PAGE_SIZE != 0) {
            LOG_ERROR(Kernel_SVC, "called, memory size not page-aligned");
            return ERR_MISALIGNED_SIZE;
        }
        break;
    case 1:
    case 3:
    case 4:
    case 5:
    case 6:
    case 7:
    case 8:
        // These are valid, but not implemented yet
        LOG_ERROR(Kernel_SVC, "unimplemented GetProcessInfo type=%u", type);
        break;
    case 20:
        *out = Memory::FCRAM_PADDR - process->GetLinearHeapAreaAddress();
        break;
    case 21:
    case 22:
    case 23:
        // These return a different error value than higher invalid values
        LOG_ERROR(Kernel_SVC, "unknown GetProcessInfo type=%u", type);
        return ERR_NOT_IMPLEMENTED;
    default:
        LOG_ERROR(Kernel_SVC, "unknown GetProcessInfo type=%u", type);
        return ERR_INVALID_ENUM_VALUE;
    }

    return RESULT_SUCCESS;
}

namespace {
struct FunctionDef {
    using Func = void();

    u32 id;
    Func* func;
    const char* name;
};
} // namespace

static const FunctionDef SVC_Table[] = {
    {0x00, nullptr, "Unknown"},
    {0x01, HLE::Wrap<ControlMemory>, "ControlMemory"},
    {0x02, HLE::Wrap<QueryMemory>, "QueryMemory"},
    {0x03, ExitProcess, "ExitProcess"},
    {0x04, nullptr, "GetProcessAffinityMask"},
    {0x05, nullptr, "SetProcessAffinityMask"},
    {0x06, nullptr, "GetProcessIdealProcessor"},
    {0x07, nullptr, "SetProcessIdealProcessor"},
    {0x08, HLE::Wrap<CreateThread>, "CreateThread"},
    {0x09, ExitThread, "ExitThread"},
    {0x0A, HLE::Wrap<SleepThread>, "SleepThread"},
    {0x0B, HLE::Wrap<GetThreadPriority>, "GetThreadPriority"},
    {0x0C, HLE::Wrap<SetThreadPriority>, "SetThreadPriority"},
    {0x0D, nullptr, "GetThreadAffinityMask"},
    {0x0E, nullptr, "SetThreadAffinityMask"},
    {0x0F, nullptr, "GetThreadIdealProcessor"},
    {0x10, nullptr, "SetThreadIdealProcessor"},
    {0x11, nullptr, "GetCurrentProcessorNumber"},
    {0x12, nullptr, "Run"},
    {0x13, HLE::Wrap<CreateMutex>, "CreateMutex"},
    {0x14, HLE::Wrap<ReleaseMutex>, "ReleaseMutex"},
    {0x15, HLE::Wrap<CreateSemaphore>, "CreateSemaphore"},
    {0x16, HLE::Wrap<ReleaseSemaphore>, "ReleaseSemaphore"},
    {0x17, HLE::Wrap<CreateEvent>, "CreateEvent"},
    {0x18, HLE::Wrap<SignalEvent>, "SignalEvent"},
    {0x19, HLE::Wrap<ClearEvent>, "ClearEvent"},
    {0x1A, HLE::Wrap<CreateTimer>, "CreateTimer"},
    {0x1B, HLE::Wrap<SetTimer>, "SetTimer"},
    {0x1C, HLE::Wrap<CancelTimer>, "CancelTimer"},
    {0x1D, HLE::Wrap<ClearTimer>, "ClearTimer"},
    {0x1E, HLE::Wrap<CreateMemoryBlock>, "CreateMemoryBlock"},
    {0x1F, HLE::Wrap<MapMemoryBlock>, "MapMemoryBlock"},
    {0x20, HLE::Wrap<UnmapMemoryBlock>, "UnmapMemoryBlock"},
    {0x21, HLE::Wrap<CreateAddressArbiter>, "CreateAddressArbiter"},
    {0x22, HLE::Wrap<ArbitrateAddress>, "ArbitrateAddress"},
    {0x23, HLE::Wrap<CloseHandle>, "CloseHandle"},
    {0x24, HLE::Wrap<WaitSynchronization1>, "WaitSynchronization1"},
    {0x25, HLE::Wrap<WaitSynchronizationN>, "WaitSynchronizationN"},
    {0x26, nullptr, "SignalAndWait"},
    {0x27, HLE::Wrap<DuplicateHandle>, "DuplicateHandle"},
    {0x28, HLE::Wrap<GetSystemTick>, "GetSystemTick"},
    {0x29, nullptr, "GetHandleInfo"},
    {0x2A, HLE::Wrap<GetSystemInfo>, "GetSystemInfo"},
    {0x2B, HLE::Wrap<GetProcessInfo>, "GetProcessInfo"},
    {0x2C, nullptr, "GetThreadInfo"},
    {0x2D, HLE::Wrap<ConnectToPort>, "ConnectToPort"},
    {0x2E, nullptr, "SendSyncRequest1"},
    {0x2F, nullptr, "SendSyncRequest2"},
    {0x30, nullptr, "SendSyncRequest3"},
    {0x31, nullptr, "SendSyncRequest4"},
    {0x32, HLE::Wrap<SendSyncRequest>, "SendSyncRequest"},
    {0x33, nullptr, "OpenProcess"},
    {0x34, nullptr, "OpenThread"},
    {0x35, HLE::Wrap<GetProcessId>, "GetProcessId"},
    {0x36, HLE::Wrap<GetProcessIdOfThread>, "GetProcessIdOfThread"},
    {0x37, HLE::Wrap<GetThreadId>, "GetThreadId"},
    {0x38, HLE::Wrap<GetResourceLimit>, "GetResourceLimit"},
    {0x39, HLE::Wrap<GetResourceLimitLimitValues>, "GetResourceLimitLimitValues"},
    {0x3A, HLE::Wrap<GetResourceLimitCurrentValues>, "GetResourceLimitCurrentValues"},
    {0x3B, nullptr, "GetThreadContext"},
    {0x3C, HLE::Wrap<Break>, "Break"},
    {0x3D, HLE::Wrap<OutputDebugString>, "OutputDebugString"},
    {0x3E, nullptr, "ControlPerformanceCounter"},
    {0x3F, nullptr, "Unknown"},
    {0x40, nullptr, "Unknown"},
    {0x41, nullptr, "Unknown"},
    {0x42, nullptr, "Unknown"},
    {0x43, nullptr, "Unknown"},
    {0x44, nullptr, "Unknown"},
    {0x45, nullptr, "Unknown"},
    {0x46, nullptr, "Unknown"},
    {0x47, HLE::Wrap<CreatePort>, "CreatePort"},
    {0x48, HLE::Wrap<CreateSessionToPort>, "CreateSessionToPort"},
    {0x49, HLE::Wrap<CreateSession>, "CreateSession"},
    {0x4A, HLE::Wrap<AcceptSession>, "AcceptSession"},
    {0x4B, nullptr, "ReplyAndReceive1"},
    {0x4C, nullptr, "ReplyAndReceive2"},
    {0x4D, nullptr, "ReplyAndReceive3"},
    {0x4E, nullptr, "ReplyAndReceive4"},
    {0x4F, HLE::Wrap<ReplyAndReceive>, "ReplyAndReceive"},
    {0x50, nullptr, "BindInterrupt"},
    {0x51, nullptr, "UnbindInterrupt"},
    {0x52, nullptr, "InvalidateProcessDataCache"},
    {0x53, nullptr, "StoreProcessDataCache"},
    {0x54, nullptr, "FlushProcessDataCache"},
    {0x55, nullptr, "StartInterProcessDma"},
    {0x56, nullptr, "StopDma"},
    {0x57, nullptr, "GetDmaState"},
    {0x58, nullptr, "RestartDma"},
    {0x59, nullptr, "SetGpuProt"},
    {0x5A, nullptr, "SetWifiEnabled"},
    {0x5B, nullptr, "Unknown"},
    {0x5C, nullptr, "Unknown"},
    {0x5D, nullptr, "Unknown"},
    {0x5E, nullptr, "Unknown"},
    {0x5F, nullptr, "Unknown"},
    {0x60, nullptr, "DebugActiveProcess"},
    {0x61, nullptr, "BreakDebugProcess"},
    {0x62, nullptr, "TerminateDebugProcess"},
    {0x63, nullptr, "GetProcessDebugEvent"},
    {0x64, nullptr, "ContinueDebugEvent"},
    {0x65, nullptr, "GetProcessList"},
    {0x66, nullptr, "GetThreadList"},
    {0x67, nullptr, "GetDebugThreadContext"},
    {0x68, nullptr, "SetDebugThreadContext"},
    {0x69, nullptr, "QueryDebugProcessMemory"},
    {0x6A, nullptr, "ReadProcessMemory"},
    {0x6B, nullptr, "WriteProcessMemory"},
    {0x6C, nullptr, "SetHardwareBreakPoint"},
    {0x6D, nullptr, "GetDebugThreadParam"},
    {0x6E, nullptr, "Unknown"},
    {0x6F, nullptr, "Unknown"},
    {0x70, nullptr, "ControlProcessMemory"},
    {0x71, nullptr, "MapProcessMemory"},
    {0x72, nullptr, "UnmapProcessMemory"},
    {0x73, nullptr, "CreateCodeSet"},
    {0x74, nullptr, "RandomStub"},
    {0x75, nullptr, "CreateProcess"},
    {0x76, nullptr, "TerminateProcess"},
    {0x77, nullptr, "SetProcessResourceLimits"},
    {0x78, nullptr, "CreateResourceLimit"},
    {0x79, nullptr, "SetResourceLimitValues"},
    {0x7A, nullptr, "AddCodeSegment"},
    {0x7B, nullptr, "Backdoor"},
    {0x7C, nullptr, "KernelSetState"},
    {0x7D, HLE::Wrap<QueryProcessMemory>, "QueryProcessMemory"},
};

static const FunctionDef* GetSVCInfo(u32 func_num) {
    if (func_num >= ARRAY_SIZE(SVC_Table)) {
        LOG_ERROR(Kernel_SVC, "unknown svc=0x%02X", func_num);
        return nullptr;
    }
    return &SVC_Table[func_num];
}

MICROPROFILE_DEFINE(Kernel_SVC, "Kernel", "SVC", MP_RGB(70, 200, 70));

void CallSVC(u32 immediate) {
    MICROPROFILE_SCOPE(Kernel_SVC);

    // Lock the global kernel mutex when we enter the kernel HLE.
    std::lock_guard<std::recursive_mutex> lock(HLE::g_hle_lock);

    ASSERT_MSG(g_current_process->status == ProcessStatus::Running,
               "Running threads from exiting processes is unimplemented");

    const FunctionDef* info = GetSVCInfo(immediate);
    if (info) {
        if (info->func) {
            info->func();
        } else {
            LOG_ERROR(Kernel_SVC, "unimplemented SVC function %s(..)", info->name);
        }
    }
}

} // namespace Kernel
