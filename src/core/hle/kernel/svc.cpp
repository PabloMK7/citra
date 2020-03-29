// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <map>
#include <fmt/format.h>
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/hle/kernel/address_arbiter.h"
#include "core/hle/kernel/client_port.h"
#include "core/hle/kernel/client_session.h"
#include "core/hle/kernel/errors.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/ipc.h"
#include "core/hle/kernel/ipc_debugger/recorder.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/kernel/mutex.h"
#include "core/hle/kernel/process.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/semaphore.h"
#include "core/hle/kernel/server_port.h"
#include "core/hle/kernel/server_session.h"
#include "core/hle/kernel/session.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/kernel/svc.h"
#include "core/hle/kernel/svc_wrapper.h"
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
    Memory::MemorySystem& memory;

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

/// Map application or GSP heap memory
ResultCode SVC::ControlMemory(u32* out_addr, u32 addr0, u32 addr1, u32 size, u32 operation,
                              u32 permissions) {
    LOG_DEBUG(Kernel_SVC,
              "called operation=0x{:08X}, addr0=0x{:08X}, addr1=0x{:08X}, "
              "size=0x{:X}, permissions=0x{:08X}",
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
        LOG_WARNING(Kernel_SVC, "ControlMemory with specified region not supported, region={:X}",
                    region);
    }

    if ((permissions & (u32)MemoryPermission::ReadWrite) != permissions) {
        return ERR_INVALID_COMBINATION;
    }
    VMAPermission vma_permissions = (VMAPermission)permissions;

    auto& process = *kernel.GetCurrentProcess();

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
        CASCADE_CODE(process.Map(addr0, addr1, size, vma_permissions));
        break;
    }

    case MEMOP_UNMAP: {
        CASCADE_CODE(process.Unmap(addr0, addr1, size, vma_permissions));
        break;
    }

    case MEMOP_PROTECT: {
        ResultCode result = process.vm_manager.ReprotectRange(addr0, size, vma_permissions);
        if (result.IsError())
            return result;
        break;
    }

    default:
        LOG_ERROR(Kernel_SVC, "unknown operation=0x{:08X}", operation);
        return ERR_INVALID_COMBINATION;
    }

    process.vm_manager.LogLayout(Log::Level::Trace);

    return RESULT_SUCCESS;
}

void SVC::ExitProcess() {
    std::shared_ptr<Process> current_process = kernel.GetCurrentProcess();
    LOG_INFO(Kernel_SVC, "Process {} exiting", current_process->process_id);

    ASSERT_MSG(current_process->status == ProcessStatus::Running, "Process has already exited");

    current_process->status = ProcessStatus::Exited;

    // Stop all the process threads that are currently waiting for objects.
    auto& thread_list = kernel.GetCurrentThreadManager().GetThreadList();
    for (auto& thread : thread_list) {
        if (thread->owner_process != current_process)
            continue;

        if (thread.get() == kernel.GetCurrentThreadManager().GetCurrentThread())
            continue;

        // TODO(Subv): When are the other running/ready threads terminated?
        ASSERT_MSG(thread->status == ThreadStatus::WaitSynchAny ||
                       thread->status == ThreadStatus::WaitSynchAll,
                   "Exiting processes with non-waiting threads is currently unimplemented");

        thread->Stop();
    }

    // Kill the current thread
    kernel.GetCurrentThreadManager().GetCurrentThread()->Stop();

    system.PrepareReschedule();
}

/// Maps a memory block to specified address
ResultCode SVC::MapMemoryBlock(Handle handle, u32 addr, u32 permissions, u32 other_permissions) {
    LOG_TRACE(Kernel_SVC,
              "called memblock=0x{:08X}, addr=0x{:08X}, mypermissions=0x{:08X}, "
              "otherpermission={}",
              handle, addr, permissions, other_permissions);

    std::shared_ptr<SharedMemory> shared_memory =
        kernel.GetCurrentProcess()->handle_table.Get<SharedMemory>(handle);
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
        return shared_memory->Map(*kernel.GetCurrentProcess(), addr, permissions_type,
                                  static_cast<MemoryPermission>(other_permissions));
    default:
        LOG_ERROR(Kernel_SVC, "unknown permissions=0x{:08X}", permissions);
    }

    return ERR_INVALID_COMBINATION;
}

ResultCode SVC::UnmapMemoryBlock(Handle handle, u32 addr) {
    LOG_TRACE(Kernel_SVC, "called memblock=0x{:08X}, addr=0x{:08X}", handle, addr);

    // TODO(Subv): Return E0A01BF5 if the address is not in the application's heap

    std::shared_ptr<Process> current_process = kernel.GetCurrentProcess();
    std::shared_ptr<SharedMemory> shared_memory =
        current_process->handle_table.Get<SharedMemory>(handle);
    if (shared_memory == nullptr)
        return ERR_INVALID_HANDLE;

    return shared_memory->Unmap(*current_process, addr);
}

/// Connect to an OS service given the port name, returns the handle to the port to out
ResultCode SVC::ConnectToPort(Handle* out_handle, VAddr port_name_address) {
    if (!Memory::IsValidVirtualAddress(*kernel.GetCurrentProcess(), port_name_address))
        return ERR_NOT_FOUND;

    static constexpr std::size_t PortNameMaxLength = 11;
    // Read 1 char beyond the max allowed port name to detect names that are too long.
    std::string port_name = memory.ReadCString(port_name_address, PortNameMaxLength + 1);
    if (port_name.size() > PortNameMaxLength)
        return ERR_PORT_NAME_TOO_LONG;

    LOG_TRACE(Kernel_SVC, "called port_name={}", port_name);

    auto it = kernel.named_ports.find(port_name);
    if (it == kernel.named_ports.end()) {
        LOG_WARNING(Kernel_SVC, "tried to connect to unknown port: {}", port_name);
        return ERR_NOT_FOUND;
    }

    auto client_port = it->second;

    std::shared_ptr<ClientSession> client_session;
    CASCADE_RESULT(client_session, client_port->Connect());

    // Return the client session
    CASCADE_RESULT(*out_handle, kernel.GetCurrentProcess()->handle_table.Create(client_session));
    return RESULT_SUCCESS;
}

/// Makes a blocking IPC call to an OS service.
ResultCode SVC::SendSyncRequest(Handle handle) {
    std::shared_ptr<ClientSession> session =
        kernel.GetCurrentProcess()->handle_table.Get<ClientSession>(handle);
    if (session == nullptr) {
        return ERR_INVALID_HANDLE;
    }

    LOG_TRACE(Kernel_SVC, "called handle=0x{:08X}({})", handle, session->GetName());

    system.PrepareReschedule();

    auto thread = SharedFrom(kernel.GetCurrentThreadManager().GetCurrentThread());

    if (kernel.GetIPCRecorder().IsEnabled()) {
        kernel.GetIPCRecorder().RegisterRequest(session, thread);
    }

    return session->SendSyncRequest(thread);
}

/// Close a handle
ResultCode SVC::CloseHandle(Handle handle) {
    LOG_TRACE(Kernel_SVC, "Closing handle 0x{:08X}", handle);
    return kernel.GetCurrentProcess()->handle_table.Close(handle);
}

static ResultCode ReceiveIPCRequest(Kernel::KernelSystem& kernel, Memory::MemorySystem& memory,
                                    std::shared_ptr<ServerSession> server_session,
                                    std::shared_ptr<Thread> thread);

class SVC_SyncCallback : public Kernel::WakeupCallback {
public:
    explicit SVC_SyncCallback(bool do_output_) : do_output(do_output_) {}
    void WakeUp(ThreadWakeupReason reason, std::shared_ptr<Thread> thread,
                std::shared_ptr<WaitObject> object) {

        if (reason == ThreadWakeupReason::Timeout) {
            thread->SetWaitSynchronizationResult(RESULT_TIMEOUT);
            return;
        }

        ASSERT(reason == ThreadWakeupReason::Signal);

        thread->SetWaitSynchronizationResult(RESULT_SUCCESS);

        // The wait_all case does not update the output index.
        if (do_output) {
            thread->SetWaitSynchronizationOutput(thread->GetWaitObjectIndex(object.get()));
        }
    }

private:
    bool do_output;

    SVC_SyncCallback() = default;
    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::WakeupCallback>(*this);
        ar& do_output;
    }
    friend class boost::serialization::access;
};

class SVC_IPCCallback : public Kernel::WakeupCallback {
public:
    explicit SVC_IPCCallback(Core::System& system_) : system(system_) {}

    void WakeUp(ThreadWakeupReason reason, std::shared_ptr<Thread> thread,
                std::shared_ptr<WaitObject> object) {

        ASSERT(thread->status == ThreadStatus::WaitSynchAny);
        ASSERT(reason == ThreadWakeupReason::Signal);

        ResultCode result = RESULT_SUCCESS;

        if (object->GetHandleType() == HandleType::ServerSession) {
            auto server_session = DynamicObjectCast<ServerSession>(object);
            result = ReceiveIPCRequest(system.Kernel(), system.Memory(), server_session, thread);
        }

        thread->SetWaitSynchronizationResult(result);
        thread->SetWaitSynchronizationOutput(thread->GetWaitObjectIndex(object.get()));
    }

private:
    Core::System& system;

    SVC_IPCCallback() : system(Core::Global<Core::System>()) {}

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        ar& boost::serialization::base_object<Kernel::WakeupCallback>(*this);
    }
    friend class boost::serialization::access;
};

/// Wait for a handle to synchronize, timeout after the specified nanoseconds
ResultCode SVC::WaitSynchronization1(Handle handle, s64 nano_seconds) {
    auto object = kernel.GetCurrentProcess()->handle_table.Get<WaitObject>(handle);
    Thread* thread = kernel.GetCurrentThreadManager().GetCurrentThread();

    if (object == nullptr)
        return ERR_INVALID_HANDLE;

    LOG_TRACE(Kernel_SVC, "called handle=0x{:08X}({}:{}), nanoseconds={}", handle,
              object->GetTypeName(), object->GetName(), nano_seconds);

    if (object->ShouldWait(thread)) {

        if (nano_seconds == 0)
            return RESULT_TIMEOUT;

        thread->wait_objects = {object};
        object->AddWaitingThread(SharedFrom(thread));
        thread->status = ThreadStatus::WaitSynchAny;

        // Create an event to wake the thread up after the specified nanosecond delay has passed
        thread->WakeAfterDelay(nano_seconds);

        thread->wakeup_callback = std::make_shared<SVC_SyncCallback>(false);

        system.PrepareReschedule();

        // Note: The output of this SVC will be set to RESULT_SUCCESS if the thread
        // resumes due to a signal in its wait objects.
        // Otherwise we retain the default value of timeout.
        return RESULT_TIMEOUT;
    }

    object->Acquire(thread);

    return RESULT_SUCCESS;
}

/// Wait for the given handles to synchronize, timeout after the specified nanoseconds
ResultCode SVC::WaitSynchronizationN(s32* out, VAddr handles_address, s32 handle_count,
                                     bool wait_all, s64 nano_seconds) {
    Thread* thread = kernel.GetCurrentThreadManager().GetCurrentThread();

    if (!Memory::IsValidVirtualAddress(*kernel.GetCurrentProcess(), handles_address))
        return ERR_INVALID_POINTER;

    // NOTE: on real hardware, there is no nullptr check for 'out' (tested with firmware 4.4). If
    // this happens, the running application will crash.
    ASSERT_MSG(out != nullptr, "invalid output pointer specified!");

    // Check if 'handle_count' is invalid
    if (handle_count < 0)
        return ERR_OUT_OF_RANGE;

    using ObjectPtr = std::shared_ptr<WaitObject>;
    std::vector<ObjectPtr> objects(handle_count);

    for (int i = 0; i < handle_count; ++i) {
        Handle handle = memory.Read32(handles_address + i * sizeof(Handle));
        auto object = kernel.GetCurrentProcess()->handle_table.Get<WaitObject>(handle);
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
        thread->status = ThreadStatus::WaitSynchAll;

        // Add the thread to each of the objects' waiting threads.
        for (auto& object : objects) {
            object->AddWaitingThread(SharedFrom(thread));
        }

        thread->wait_objects = std::move(objects);

        // Create an event to wake the thread up after the specified nanosecond delay has passed
        thread->WakeAfterDelay(nano_seconds);

        thread->wakeup_callback = std::make_shared<SVC_SyncCallback>(false);

        system.PrepareReschedule();

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
        thread->status = ThreadStatus::WaitSynchAny;

        // Add the thread to each of the objects' waiting threads.
        for (std::size_t i = 0; i < objects.size(); ++i) {
            WaitObject* object = objects[i].get();
            object->AddWaitingThread(SharedFrom(thread));
        }

        thread->wait_objects = std::move(objects);

        // Note: If no handles and no timeout were given, then the thread will deadlock, this is
        // consistent with hardware behavior.

        // Create an event to wake the thread up after the specified nanosecond delay has passed
        thread->WakeAfterDelay(nano_seconds);

        thread->wakeup_callback = std::make_shared<SVC_SyncCallback>(true);

        system.PrepareReschedule();

        // Note: The output of this SVC will be set to RESULT_SUCCESS if the thread resumes due to a
        // signal in one of its wait objects.
        // Otherwise we retain the default value of timeout, and -1 in the out parameter
        *out = -1;
        return RESULT_TIMEOUT;
    }
}

static ResultCode ReceiveIPCRequest(Kernel::KernelSystem& kernel, Memory::MemorySystem& memory,
                                    std::shared_ptr<ServerSession> server_session,
                                    std::shared_ptr<Thread> thread) {
    if (server_session->parent->client == nullptr) {
        return ERR_SESSION_CLOSED_BY_REMOTE;
    }

    VAddr target_address = thread->GetCommandBufferAddress();
    VAddr source_address = server_session->currently_handling->GetCommandBufferAddress();

    ResultCode translation_result = TranslateCommandBuffer(
        kernel, memory, server_session->currently_handling, thread, source_address, target_address,
        server_session->mapped_buffer_context, false);

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
ResultCode SVC::ReplyAndReceive(s32* index, VAddr handles_address, s32 handle_count,
                                Handle reply_target) {
    if (!Memory::IsValidVirtualAddress(*kernel.GetCurrentProcess(), handles_address))
        return ERR_INVALID_POINTER;

    // Check if 'handle_count' is invalid
    if (handle_count < 0)
        return ERR_OUT_OF_RANGE;

    using ObjectPtr = std::shared_ptr<WaitObject>;
    std::vector<ObjectPtr> objects(handle_count);

    std::shared_ptr<Process> current_process = kernel.GetCurrentProcess();

    for (int i = 0; i < handle_count; ++i) {
        Handle handle = memory.Read32(handles_address + i * sizeof(Handle));
        auto object = current_process->handle_table.Get<WaitObject>(handle);
        if (object == nullptr)
            return ERR_INVALID_HANDLE;
        objects[i] = object;
    }

    // We are also sending a command reply.
    // Do not send a reply if the command id in the command buffer is 0xFFFF.
    Thread* thread = kernel.GetCurrentThreadManager().GetCurrentThread();
    u32 cmd_buff_header = memory.Read32(thread->GetCommandBufferAddress());
    IPC::Header header{cmd_buff_header};
    if (reply_target != 0 && header.command_id != 0xFFFF) {
        auto session = current_process->handle_table.Get<ServerSession>(reply_target);
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

        VAddr source_address = thread->GetCommandBufferAddress();
        VAddr target_address = request_thread->GetCommandBufferAddress();

        ResultCode translation_result = TranslateCommandBuffer(
            kernel, memory, SharedFrom(thread), request_thread, source_address, target_address,
            session->mapped_buffer_context, true);

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
        return ReceiveIPCRequest(kernel, memory, SharedFrom(server_session), SharedFrom(thread));
    }

    // No objects were ready to be acquired, prepare to suspend the thread.

    // Put the thread to sleep
    thread->status = ThreadStatus::WaitSynchAny;

    // Add the thread to each of the objects' waiting threads.
    for (std::size_t i = 0; i < objects.size(); ++i) {
        WaitObject* object = objects[i].get();
        object->AddWaitingThread(SharedFrom(thread));
    }

    thread->wait_objects = std::move(objects);

    thread->wakeup_callback = std::make_shared<SVC_IPCCallback>(system);

    system.PrepareReschedule();

    // Note: The output of this SVC will be set to RESULT_SUCCESS if the thread resumes due to a
    // signal in one of its wait objects, or to 0xC8A01836 if there was a translation error.
    // By default the index is set to -1.
    *index = -1;
    return RESULT_SUCCESS;
}

/// Create an address arbiter (to allocate access to shared resources)
ResultCode SVC::CreateAddressArbiter(Handle* out_handle) {
    std::shared_ptr<AddressArbiter> arbiter = kernel.CreateAddressArbiter();
    CASCADE_RESULT(*out_handle,
                   kernel.GetCurrentProcess()->handle_table.Create(std::move(arbiter)));
    LOG_TRACE(Kernel_SVC, "returned handle=0x{:08X}", *out_handle);
    return RESULT_SUCCESS;
}

/// Arbitrate address
ResultCode SVC::ArbitrateAddress(Handle handle, u32 address, u32 type, u32 value, s64 nanoseconds) {
    LOG_TRACE(Kernel_SVC, "called handle=0x{:08X}, address=0x{:08X}, type=0x{:08X}, value=0x{:08X}",
              handle, address, type, value);

    std::shared_ptr<AddressArbiter> arbiter =
        kernel.GetCurrentProcess()->handle_table.Get<AddressArbiter>(handle);
    if (arbiter == nullptr)
        return ERR_INVALID_HANDLE;

    auto res =
        arbiter->ArbitrateAddress(SharedFrom(kernel.GetCurrentThreadManager().GetCurrentThread()),
                                  static_cast<ArbitrationType>(type), address, value, nanoseconds);

    // TODO(Subv): Identify in which specific cases this call should cause a reschedule.
    system.PrepareReschedule();

    return res;
}

void SVC::Break(u8 break_reason) {
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
    LOG_CRITICAL(Debug_Emulated, "Break reason: {}", reason_str);
}

/// Used to output a message on a debug hardware unit - does nothing on a retail unit
void SVC::OutputDebugString(VAddr address, s32 len) {
    if (len <= 0) {
        return;
    }

    std::string string(len, ' ');
    memory.ReadBlock(*kernel.GetCurrentProcess(), address, string.data(), len);
    LOG_DEBUG(Debug_Emulated, "{}", string);
}

/// Get resource limit
ResultCode SVC::GetResourceLimit(Handle* resource_limit, Handle process_handle) {
    LOG_TRACE(Kernel_SVC, "called process=0x{:08X}", process_handle);

    std::shared_ptr<Process> current_process = kernel.GetCurrentProcess();
    std::shared_ptr<Process> process = current_process->handle_table.Get<Process>(process_handle);
    if (process == nullptr)
        return ERR_INVALID_HANDLE;

    CASCADE_RESULT(*resource_limit, current_process->handle_table.Create(process->resource_limit));

    return RESULT_SUCCESS;
}

/// Get resource limit current values
ResultCode SVC::GetResourceLimitCurrentValues(VAddr values, Handle resource_limit_handle,
                                              VAddr names, u32 name_count) {
    LOG_TRACE(Kernel_SVC, "called resource_limit={:08X}, names={:08X}, name_count={}",
              resource_limit_handle, names, name_count);

    std::shared_ptr<ResourceLimit> resource_limit =
        kernel.GetCurrentProcess()->handle_table.Get<ResourceLimit>(resource_limit_handle);
    if (resource_limit == nullptr)
        return ERR_INVALID_HANDLE;

    for (unsigned int i = 0; i < name_count; ++i) {
        u32 name = memory.Read32(names + i * sizeof(u32));
        s64 value = resource_limit->GetCurrentResourceValue(name);
        memory.Write64(values + i * sizeof(u64), value);
    }

    return RESULT_SUCCESS;
}

/// Get resource limit max values
ResultCode SVC::GetResourceLimitLimitValues(VAddr values, Handle resource_limit_handle, VAddr names,
                                            u32 name_count) {
    LOG_TRACE(Kernel_SVC, "called resource_limit={:08X}, names={:08X}, name_count={}",
              resource_limit_handle, names, name_count);

    std::shared_ptr<ResourceLimit> resource_limit =
        kernel.GetCurrentProcess()->handle_table.Get<ResourceLimit>(resource_limit_handle);
    if (resource_limit == nullptr)
        return ERR_INVALID_HANDLE;

    for (unsigned int i = 0; i < name_count; ++i) {
        u32 name = memory.Read32(names + i * sizeof(u32));
        s64 value = resource_limit->GetMaxResourceValue(name);
        memory.Write64(values + i * sizeof(u64), value);
    }

    return RESULT_SUCCESS;
}

/// Creates a new thread
ResultCode SVC::CreateThread(Handle* out_handle, u32 entry_point, u32 arg, VAddr stack_top,
                             u32 priority, s32 processor_id) {
    std::string name = fmt::format("thread-{:08X}", entry_point);

    if (priority > ThreadPrioLowest) {
        return ERR_OUT_OF_RANGE;
    }

    std::shared_ptr<Process> current_process = kernel.GetCurrentProcess();

    std::shared_ptr<ResourceLimit>& resource_limit = current_process->resource_limit;
    if (resource_limit->GetMaxResourceValue(ResourceTypes::PRIORITY) > priority) {
        return ERR_NOT_AUTHORIZED;
    }

    if (processor_id == ThreadProcessorIdDefault) {
        // Set the target CPU to the one specified in the process' exheader.
        processor_id = current_process->ideal_processor;
        ASSERT(processor_id != ThreadProcessorIdDefault);
    }

    switch (processor_id) {
    case ThreadProcessorId0:
        break;
    case ThreadProcessorIdAll:
        LOG_INFO(Kernel_SVC,
                 "Newly created thread is allowed to be run in any Core, for now run in core 0.");
        processor_id = ThreadProcessorId0;
        break;
    case ThreadProcessorId1:
    case ThreadProcessorId2:
    case ThreadProcessorId3:
        // TODO: Check and log for: When processorid==0x2 and the process is not a BASE mem-region
        // process, exheader kernel-flags bitmask 0x2000 must be set (otherwise error 0xD9001BEA is
        // returned). When processorid==0x3 and the process is not a BASE mem-region process, error
        // 0xD9001BEA is returned. These are the only restriction checks done by the kernel for
        // processorid.
        break;
    default:
        ASSERT_MSG(false, "Unsupported thread processor ID: {}", processor_id);
        break;
    }

    CASCADE_RESULT(std::shared_ptr<Thread> thread,
                   kernel.CreateThread(name, entry_point, priority, arg, processor_id, stack_top,
                                       current_process));

    thread->context->SetFpscr(FPSCR_DEFAULT_NAN | FPSCR_FLUSH_TO_ZERO |
                              FPSCR_ROUND_TOZERO); // 0x03C00000

    CASCADE_RESULT(*out_handle, current_process->handle_table.Create(std::move(thread)));

    system.PrepareReschedule();

    LOG_TRACE(Kernel_SVC,
              "called entrypoint=0x{:08X} ({}), arg=0x{:08X}, stacktop=0x{:08X}, "
              "threadpriority=0x{:08X}, processorid=0x{:08X} : created handle=0x{:08X}",
              entry_point, name, arg, stack_top, priority, processor_id, *out_handle);

    return RESULT_SUCCESS;
}

/// Called when a thread exits
void SVC::ExitThread() {
    LOG_TRACE(Kernel_SVC, "called, pc=0x{:08X}", system.GetRunningCore().GetPC());

    kernel.GetCurrentThreadManager().ExitCurrentThread();
    system.PrepareReschedule();
}

/// Gets the priority for the specified thread
ResultCode SVC::GetThreadPriority(u32* priority, Handle handle) {
    const std::shared_ptr<Thread> thread =
        kernel.GetCurrentProcess()->handle_table.Get<Thread>(handle);
    if (thread == nullptr)
        return ERR_INVALID_HANDLE;

    *priority = thread->GetPriority();
    return RESULT_SUCCESS;
}

/// Sets the priority for the specified thread
ResultCode SVC::SetThreadPriority(Handle handle, u32 priority) {
    if (priority > ThreadPrioLowest) {
        return ERR_OUT_OF_RANGE;
    }

    std::shared_ptr<Thread> thread = kernel.GetCurrentProcess()->handle_table.Get<Thread>(handle);
    if (thread == nullptr)
        return ERR_INVALID_HANDLE;

    // Note: The kernel uses the current process's resource limit instead of
    // the one from the thread owner's resource limit.
    std::shared_ptr<ResourceLimit>& resource_limit = kernel.GetCurrentProcess()->resource_limit;
    if (resource_limit->GetMaxResourceValue(ResourceTypes::PRIORITY) > priority) {
        return ERR_NOT_AUTHORIZED;
    }

    thread->SetPriority(priority);
    thread->UpdatePriority();

    // Update the mutexes that this thread is waiting for
    for (auto& mutex : thread->pending_mutexes)
        mutex->UpdatePriority();

    system.PrepareReschedule();
    return RESULT_SUCCESS;
}

/// Create a mutex
ResultCode SVC::CreateMutex(Handle* out_handle, u32 initial_locked) {
    std::shared_ptr<Mutex> mutex = kernel.CreateMutex(initial_locked != 0);
    mutex->name = fmt::format("mutex-{:08x}", system.GetRunningCore().GetReg(14));
    CASCADE_RESULT(*out_handle, kernel.GetCurrentProcess()->handle_table.Create(std::move(mutex)));

    LOG_TRACE(Kernel_SVC, "called initial_locked={} : created handle=0x{:08X}",
              initial_locked ? "true" : "false", *out_handle);

    return RESULT_SUCCESS;
}

/// Release a mutex
ResultCode SVC::ReleaseMutex(Handle handle) {
    LOG_TRACE(Kernel_SVC, "called handle=0x{:08X}", handle);

    std::shared_ptr<Mutex> mutex = kernel.GetCurrentProcess()->handle_table.Get<Mutex>(handle);
    if (mutex == nullptr)
        return ERR_INVALID_HANDLE;

    return mutex->Release(kernel.GetCurrentThreadManager().GetCurrentThread());
}

/// Get the ID of the specified process
ResultCode SVC::GetProcessId(u32* process_id, Handle process_handle) {
    LOG_TRACE(Kernel_SVC, "called process=0x{:08X}", process_handle);

    const std::shared_ptr<Process> process =
        kernel.GetCurrentProcess()->handle_table.Get<Process>(process_handle);
    if (process == nullptr)
        return ERR_INVALID_HANDLE;

    *process_id = process->process_id;
    return RESULT_SUCCESS;
}

/// Get the ID of the process that owns the specified thread
ResultCode SVC::GetProcessIdOfThread(u32* process_id, Handle thread_handle) {
    LOG_TRACE(Kernel_SVC, "called thread=0x{:08X}", thread_handle);

    const std::shared_ptr<Thread> thread =
        kernel.GetCurrentProcess()->handle_table.Get<Thread>(thread_handle);
    if (thread == nullptr)
        return ERR_INVALID_HANDLE;

    const std::shared_ptr<Process> process = thread->owner_process;

    ASSERT_MSG(process != nullptr, "Invalid parent process for thread={:#010X}", thread_handle);

    *process_id = process->process_id;
    return RESULT_SUCCESS;
}

/// Get the ID for the specified thread.
ResultCode SVC::GetThreadId(u32* thread_id, Handle handle) {
    LOG_TRACE(Kernel_SVC, "called thread=0x{:08X}", handle);

    const std::shared_ptr<Thread> thread =
        kernel.GetCurrentProcess()->handle_table.Get<Thread>(handle);
    if (thread == nullptr)
        return ERR_INVALID_HANDLE;

    *thread_id = thread->GetThreadId();
    return RESULT_SUCCESS;
}

/// Creates a semaphore
ResultCode SVC::CreateSemaphore(Handle* out_handle, s32 initial_count, s32 max_count) {
    CASCADE_RESULT(std::shared_ptr<Semaphore> semaphore,
                   kernel.CreateSemaphore(initial_count, max_count));
    semaphore->name = fmt::format("semaphore-{:08x}", system.GetRunningCore().GetReg(14));
    CASCADE_RESULT(*out_handle,
                   kernel.GetCurrentProcess()->handle_table.Create(std::move(semaphore)));

    LOG_TRACE(Kernel_SVC, "called initial_count={}, max_count={}, created handle=0x{:08X}",
              initial_count, max_count, *out_handle);
    return RESULT_SUCCESS;
}

/// Releases a certain number of slots in a semaphore
ResultCode SVC::ReleaseSemaphore(s32* count, Handle handle, s32 release_count) {
    LOG_TRACE(Kernel_SVC, "called release_count={}, handle=0x{:08X}", release_count, handle);

    std::shared_ptr<Semaphore> semaphore =
        kernel.GetCurrentProcess()->handle_table.Get<Semaphore>(handle);
    if (semaphore == nullptr)
        return ERR_INVALID_HANDLE;

    CASCADE_RESULT(*count, semaphore->Release(release_count));

    return RESULT_SUCCESS;
}

/// Query process memory
ResultCode SVC::QueryProcessMemory(MemoryInfo* memory_info, PageInfo* page_info,
                                   Handle process_handle, u32 addr) {
    std::shared_ptr<Process> process =
        kernel.GetCurrentProcess()->handle_table.Get<Process>(process_handle);
    if (process == nullptr)
        return ERR_INVALID_HANDLE;

    auto vma = process->vm_manager.FindVMA(addr);

    if (vma == process->vm_manager.vma_map.end())
        return ERR_INVALID_ADDRESS;

    auto permissions = vma->second.permissions;
    auto state = vma->second.meminfo_state;

    // Query(Process)Memory merges vma with neighbours when they share the same state and
    // permissions, regardless of their physical mapping.

    auto mismatch = [permissions, state](const std::pair<VAddr, Kernel::VirtualMemoryArea>& v) {
        return v.second.permissions != permissions || v.second.meminfo_state != state;
    };

    std::reverse_iterator rvma(vma);

    auto lower = std::find_if(rvma, process->vm_manager.vma_map.crend(), mismatch);
    --lower;
    auto upper = std::find_if(vma, process->vm_manager.vma_map.cend(), mismatch);
    --upper;

    memory_info->base_address = lower->second.base;
    memory_info->permission = static_cast<u32>(permissions);
    memory_info->size = upper->second.base + upper->second.size - lower->second.base;
    memory_info->state = static_cast<u32>(state);

    page_info->flags = 0;
    LOG_TRACE(Kernel_SVC, "called process=0x{:08X} addr=0x{:08X}", process_handle, addr);
    return RESULT_SUCCESS;
}

/// Query memory
ResultCode SVC::QueryMemory(MemoryInfo* memory_info, PageInfo* page_info, u32 addr) {
    return QueryProcessMemory(memory_info, page_info, CurrentProcess, addr);
}

/// Create an event
ResultCode SVC::CreateEvent(Handle* out_handle, u32 reset_type) {
    std::shared_ptr<Event> evt =
        kernel.CreateEvent(static_cast<ResetType>(reset_type),
                           fmt::format("event-{:08x}", system.GetRunningCore().GetReg(14)));
    CASCADE_RESULT(*out_handle, kernel.GetCurrentProcess()->handle_table.Create(std::move(evt)));

    LOG_TRACE(Kernel_SVC, "called reset_type=0x{:08X} : created handle=0x{:08X}", reset_type,
              *out_handle);
    return RESULT_SUCCESS;
}

/// Duplicates a kernel handle
ResultCode SVC::DuplicateHandle(Handle* out, Handle handle) {
    CASCADE_RESULT(*out, kernel.GetCurrentProcess()->handle_table.Duplicate(handle));
    LOG_TRACE(Kernel_SVC, "duplicated 0x{:08X} to 0x{:08X}", handle, *out);
    return RESULT_SUCCESS;
}

/// Signals an event
ResultCode SVC::SignalEvent(Handle handle) {
    LOG_TRACE(Kernel_SVC, "called event=0x{:08X}", handle);

    std::shared_ptr<Event> evt = kernel.GetCurrentProcess()->handle_table.Get<Event>(handle);
    if (evt == nullptr)
        return ERR_INVALID_HANDLE;

    evt->Signal();

    return RESULT_SUCCESS;
}

/// Clears an event
ResultCode SVC::ClearEvent(Handle handle) {
    LOG_TRACE(Kernel_SVC, "called event=0x{:08X}", handle);

    std::shared_ptr<Event> evt = kernel.GetCurrentProcess()->handle_table.Get<Event>(handle);
    if (evt == nullptr)
        return ERR_INVALID_HANDLE;

    evt->Clear();
    return RESULT_SUCCESS;
}

/// Creates a timer
ResultCode SVC::CreateTimer(Handle* out_handle, u32 reset_type) {
    std::shared_ptr<Timer> timer =
        kernel.CreateTimer(static_cast<ResetType>(reset_type),
                           fmt ::format("timer-{:08x}", system.GetRunningCore().GetReg(14)));
    CASCADE_RESULT(*out_handle, kernel.GetCurrentProcess()->handle_table.Create(std::move(timer)));

    LOG_TRACE(Kernel_SVC, "called reset_type=0x{:08X} : created handle=0x{:08X}", reset_type,
              *out_handle);
    return RESULT_SUCCESS;
}

/// Clears a timer
ResultCode SVC::ClearTimer(Handle handle) {
    LOG_TRACE(Kernel_SVC, "called timer=0x{:08X}", handle);

    std::shared_ptr<Timer> timer = kernel.GetCurrentProcess()->handle_table.Get<Timer>(handle);
    if (timer == nullptr)
        return ERR_INVALID_HANDLE;

    timer->Clear();
    return RESULT_SUCCESS;
}

/// Starts a timer
ResultCode SVC::SetTimer(Handle handle, s64 initial, s64 interval) {
    LOG_TRACE(Kernel_SVC, "called timer=0x{:08X}", handle);

    if (initial < 0 || interval < 0) {
        return ERR_OUT_OF_RANGE_KERNEL;
    }

    std::shared_ptr<Timer> timer = kernel.GetCurrentProcess()->handle_table.Get<Timer>(handle);
    if (timer == nullptr)
        return ERR_INVALID_HANDLE;

    timer->Set(initial, interval);

    return RESULT_SUCCESS;
}

/// Cancels a timer
ResultCode SVC::CancelTimer(Handle handle) {
    LOG_TRACE(Kernel_SVC, "called timer=0x{:08X}", handle);

    std::shared_ptr<Timer> timer = kernel.GetCurrentProcess()->handle_table.Get<Timer>(handle);
    if (timer == nullptr)
        return ERR_INVALID_HANDLE;

    timer->Cancel();

    return RESULT_SUCCESS;
}

/// Sleep the current thread
void SVC::SleepThread(s64 nanoseconds) {
    LOG_TRACE(Kernel_SVC, "called nanoseconds={}", nanoseconds);

    ThreadManager& thread_manager = kernel.GetCurrentThreadManager();

    // Don't attempt to yield execution if there are no available threads to run,
    // this way we avoid a useless reschedule to the idle thread.
    if (nanoseconds == 0 && !thread_manager.HaveReadyThreads())
        return;

    // Sleep current thread and check for next thread to schedule
    thread_manager.WaitCurrentThread_Sleep();

    // Create an event to wake the thread up after the specified nanosecond delay has passed
    thread_manager.GetCurrentThread()->WakeAfterDelay(nanoseconds);

    system.PrepareReschedule();
}

/// This returns the total CPU ticks elapsed since the CPU was powered-on
s64 SVC::GetSystemTick() {
    // TODO: Use globalTicks here?
    s64 result = system.GetRunningCore().GetTimer()->GetTicks();
    // Advance time to defeat dumb games (like Cubic Ninja) that busy-wait for the frame to end.
    // Measured time between two calls on a 9.2 o3DS with Ninjhax 1.1b
    system.GetRunningCore().GetTimer()->AddTicks(150);
    return result;
}

/// Creates a memory block at the specified address with the specified permissions and size
ResultCode SVC::CreateMemoryBlock(Handle* out_handle, u32 addr, u32 size, u32 my_permission,
                                  u32 other_permission) {
    if (size % Memory::PAGE_SIZE != 0)
        return ERR_MISALIGNED_SIZE;

    std::shared_ptr<SharedMemory> shared_memory = nullptr;

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

    std::shared_ptr<Process> current_process = kernel.GetCurrentProcess();

    // When trying to create a memory block with address = 0,
    // if the process has the Shared Device Memory flag in the exheader,
    // then we have to allocate from the same region as the caller process instead of the BASE
    // region.
    MemoryRegion region = MemoryRegion::BASE;
    if (addr == 0 && current_process->flags.shared_device_mem)
        region = current_process->flags.memory_region;

    CASCADE_RESULT(shared_memory,
                   kernel.CreateSharedMemory(
                       current_process.get(), size, static_cast<MemoryPermission>(my_permission),
                       static_cast<MemoryPermission>(other_permission), addr, region));
    CASCADE_RESULT(*out_handle, current_process->handle_table.Create(std::move(shared_memory)));

    LOG_WARNING(Kernel_SVC, "called addr=0x{:08X}", addr);
    return RESULT_SUCCESS;
}

ResultCode SVC::CreatePort(Handle* server_port, Handle* client_port, VAddr name_address,
                           u32 max_sessions) {
    // TODO(Subv): Implement named ports.
    ASSERT_MSG(name_address == 0, "Named ports are currently unimplemented");

    std::shared_ptr<Process> current_process = kernel.GetCurrentProcess();

    auto [server, client] = kernel.CreatePortPair(max_sessions);
    CASCADE_RESULT(*client_port, current_process->handle_table.Create(std::move(client)));
    // Note: The 3DS kernel also leaks the client port handle if the server port handle fails to be
    // created.
    CASCADE_RESULT(*server_port, current_process->handle_table.Create(std::move(server)));

    LOG_TRACE(Kernel_SVC, "called max_sessions={}", max_sessions);
    return RESULT_SUCCESS;
}

ResultCode SVC::CreateSessionToPort(Handle* out_client_session, Handle client_port_handle) {
    std::shared_ptr<Process> current_process = kernel.GetCurrentProcess();
    std::shared_ptr<ClientPort> client_port =
        current_process->handle_table.Get<ClientPort>(client_port_handle);
    if (client_port == nullptr)
        return ERR_INVALID_HANDLE;

    CASCADE_RESULT(auto session, client_port->Connect());
    CASCADE_RESULT(*out_client_session, current_process->handle_table.Create(std::move(session)));
    return RESULT_SUCCESS;
}

ResultCode SVC::CreateSession(Handle* server_session, Handle* client_session) {
    auto [server, client] = kernel.CreateSessionPair();

    std::shared_ptr<Process> current_process = kernel.GetCurrentProcess();

    CASCADE_RESULT(*server_session, current_process->handle_table.Create(std::move(server)));

    CASCADE_RESULT(*client_session, current_process->handle_table.Create(std::move(client)));

    LOG_TRACE(Kernel_SVC, "called");
    return RESULT_SUCCESS;
}

ResultCode SVC::AcceptSession(Handle* out_server_session, Handle server_port_handle) {
    std::shared_ptr<Process> current_process = kernel.GetCurrentProcess();
    std::shared_ptr<ServerPort> server_port =
        current_process->handle_table.Get<ServerPort>(server_port_handle);
    if (server_port == nullptr)
        return ERR_INVALID_HANDLE;

    CASCADE_RESULT(auto session, server_port->Accept());
    CASCADE_RESULT(*out_server_session, current_process->handle_table.Create(std::move(session)));
    return RESULT_SUCCESS;
}

ResultCode SVC::GetSystemInfo(s64* out, u32 type, s32 param) {
    LOG_TRACE(Kernel_SVC, "called type={} param={}", type, param);

    switch ((SystemInfoType)type) {
    case SystemInfoType::REGION_MEMORY_USAGE:
        switch ((SystemInfoMemUsageRegion)param) {
        case SystemInfoMemUsageRegion::ALL:
            *out = kernel.GetMemoryRegion(MemoryRegion::APPLICATION)->used +
                   kernel.GetMemoryRegion(MemoryRegion::SYSTEM)->used +
                   kernel.GetMemoryRegion(MemoryRegion::BASE)->used;
            break;
        case SystemInfoMemUsageRegion::APPLICATION:
            *out = kernel.GetMemoryRegion(MemoryRegion::APPLICATION)->used;
            break;
        case SystemInfoMemUsageRegion::SYSTEM:
            *out = kernel.GetMemoryRegion(MemoryRegion::SYSTEM)->used;
            break;
        case SystemInfoMemUsageRegion::BASE:
            *out = kernel.GetMemoryRegion(MemoryRegion::BASE)->used;
            break;
        default:
            LOG_ERROR(Kernel_SVC, "unknown GetSystemInfo type=0 region: param={}", param);
            *out = 0;
            break;
        }
        break;
    case SystemInfoType::KERNEL_ALLOCATED_PAGES:
        LOG_ERROR(Kernel_SVC, "unimplemented GetSystemInfo type=2 param={}", param);
        *out = 0;
        break;
    case SystemInfoType::KERNEL_SPAWNED_PIDS:
        *out = 5;
        break;
    default:
        LOG_ERROR(Kernel_SVC, "unknown GetSystemInfo type={} param={}", type, param);
        *out = 0;
        break;
    }

    // This function never returns an error, even if invalid parameters were passed.
    return RESULT_SUCCESS;
}

ResultCode SVC::GetProcessInfo(s64* out, Handle process_handle, u32 type) {
    LOG_TRACE(Kernel_SVC, "called process=0x{:08X} type={}", process_handle, type);

    std::shared_ptr<Process> process =
        kernel.GetCurrentProcess()->handle_table.Get<Process>(process_handle);
    if (process == nullptr)
        return ERR_INVALID_HANDLE;

    switch (type) {
    case 0:
    case 2:
        // TODO(yuriks): Type 0 returns a slightly higher number than type 2, but I'm not sure
        // what's the difference between them.
        *out = process->memory_used;
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
        LOG_ERROR(Kernel_SVC, "unimplemented GetProcessInfo type={}", type);
        break;
    case 20:
        *out = Memory::FCRAM_PADDR - process->GetLinearHeapAreaAddress();
        break;
    case 21:
    case 22:
    case 23:
        // These return a different error value than higher invalid values
        LOG_ERROR(Kernel_SVC, "unknown GetProcessInfo type={}", type);
        return ERR_NOT_IMPLEMENTED;
    default:
        LOG_ERROR(Kernel_SVC, "unknown GetProcessInfo type={}", type);
        return ERR_INVALID_ENUM_VALUE;
    }

    return RESULT_SUCCESS;
}

const SVC::FunctionDef SVC::SVC_Table[] = {
    {0x00, nullptr, "Unknown"},
    {0x01, &SVC::Wrap<&SVC::ControlMemory>, "ControlMemory"},
    {0x02, &SVC::Wrap<&SVC::QueryMemory>, "QueryMemory"},
    {0x03, &SVC::ExitProcess, "ExitProcess"},
    {0x04, nullptr, "GetProcessAffinityMask"},
    {0x05, nullptr, "SetProcessAffinityMask"},
    {0x06, nullptr, "GetProcessIdealProcessor"},
    {0x07, nullptr, "SetProcessIdealProcessor"},
    {0x08, &SVC::Wrap<&SVC::CreateThread>, "CreateThread"},
    {0x09, &SVC::ExitThread, "ExitThread"},
    {0x0A, &SVC::Wrap<&SVC::SleepThread>, "SleepThread"},
    {0x0B, &SVC::Wrap<&SVC::GetThreadPriority>, "GetThreadPriority"},
    {0x0C, &SVC::Wrap<&SVC::SetThreadPriority>, "SetThreadPriority"},
    {0x0D, nullptr, "GetThreadAffinityMask"},
    {0x0E, nullptr, "SetThreadAffinityMask"},
    {0x0F, nullptr, "GetThreadIdealProcessor"},
    {0x10, nullptr, "SetThreadIdealProcessor"},
    {0x11, nullptr, "GetCurrentProcessorNumber"},
    {0x12, nullptr, "Run"},
    {0x13, &SVC::Wrap<&SVC::CreateMutex>, "CreateMutex"},
    {0x14, &SVC::Wrap<&SVC::ReleaseMutex>, "ReleaseMutex"},
    {0x15, &SVC::Wrap<&SVC::CreateSemaphore>, "CreateSemaphore"},
    {0x16, &SVC::Wrap<&SVC::ReleaseSemaphore>, "ReleaseSemaphore"},
    {0x17, &SVC::Wrap<&SVC::CreateEvent>, "CreateEvent"},
    {0x18, &SVC::Wrap<&SVC::SignalEvent>, "SignalEvent"},
    {0x19, &SVC::Wrap<&SVC::ClearEvent>, "ClearEvent"},
    {0x1A, &SVC::Wrap<&SVC::CreateTimer>, "CreateTimer"},
    {0x1B, &SVC::Wrap<&SVC::SetTimer>, "SetTimer"},
    {0x1C, &SVC::Wrap<&SVC::CancelTimer>, "CancelTimer"},
    {0x1D, &SVC::Wrap<&SVC::ClearTimer>, "ClearTimer"},
    {0x1E, &SVC::Wrap<&SVC::CreateMemoryBlock>, "CreateMemoryBlock"},
    {0x1F, &SVC::Wrap<&SVC::MapMemoryBlock>, "MapMemoryBlock"},
    {0x20, &SVC::Wrap<&SVC::UnmapMemoryBlock>, "UnmapMemoryBlock"},
    {0x21, &SVC::Wrap<&SVC::CreateAddressArbiter>, "CreateAddressArbiter"},
    {0x22, &SVC::Wrap<&SVC::ArbitrateAddress>, "ArbitrateAddress"},
    {0x23, &SVC::Wrap<&SVC::CloseHandle>, "CloseHandle"},
    {0x24, &SVC::Wrap<&SVC::WaitSynchronization1>, "WaitSynchronization1"},
    {0x25, &SVC::Wrap<&SVC::WaitSynchronizationN>, "WaitSynchronizationN"},
    {0x26, nullptr, "SignalAndWait"},
    {0x27, &SVC::Wrap<&SVC::DuplicateHandle>, "DuplicateHandle"},
    {0x28, &SVC::Wrap<&SVC::GetSystemTick>, "GetSystemTick"},
    {0x29, nullptr, "GetHandleInfo"},
    {0x2A, &SVC::Wrap<&SVC::GetSystemInfo>, "GetSystemInfo"},
    {0x2B, &SVC::Wrap<&SVC::GetProcessInfo>, "GetProcessInfo"},
    {0x2C, nullptr, "GetThreadInfo"},
    {0x2D, &SVC::Wrap<&SVC::ConnectToPort>, "ConnectToPort"},
    {0x2E, nullptr, "SendSyncRequest1"},
    {0x2F, nullptr, "SendSyncRequest2"},
    {0x30, nullptr, "SendSyncRequest3"},
    {0x31, nullptr, "SendSyncRequest4"},
    {0x32, &SVC::Wrap<&SVC::SendSyncRequest>, "SendSyncRequest"},
    {0x33, nullptr, "OpenProcess"},
    {0x34, nullptr, "OpenThread"},
    {0x35, &SVC::Wrap<&SVC::GetProcessId>, "GetProcessId"},
    {0x36, &SVC::Wrap<&SVC::GetProcessIdOfThread>, "GetProcessIdOfThread"},
    {0x37, &SVC::Wrap<&SVC::GetThreadId>, "GetThreadId"},
    {0x38, &SVC::Wrap<&SVC::GetResourceLimit>, "GetResourceLimit"},
    {0x39, &SVC::Wrap<&SVC::GetResourceLimitLimitValues>, "GetResourceLimitLimitValues"},
    {0x3A, &SVC::Wrap<&SVC::GetResourceLimitCurrentValues>, "GetResourceLimitCurrentValues"},
    {0x3B, nullptr, "GetThreadContext"},
    {0x3C, &SVC::Wrap<&SVC::Break>, "Break"},
    {0x3D, &SVC::Wrap<&SVC::OutputDebugString>, "OutputDebugString"},
    {0x3E, nullptr, "ControlPerformanceCounter"},
    {0x3F, nullptr, "Unknown"},
    {0x40, nullptr, "Unknown"},
    {0x41, nullptr, "Unknown"},
    {0x42, nullptr, "Unknown"},
    {0x43, nullptr, "Unknown"},
    {0x44, nullptr, "Unknown"},
    {0x45, nullptr, "Unknown"},
    {0x46, nullptr, "Unknown"},
    {0x47, &SVC::Wrap<&SVC::CreatePort>, "CreatePort"},
    {0x48, &SVC::Wrap<&SVC::CreateSessionToPort>, "CreateSessionToPort"},
    {0x49, &SVC::Wrap<&SVC::CreateSession>, "CreateSession"},
    {0x4A, &SVC::Wrap<&SVC::AcceptSession>, "AcceptSession"},
    {0x4B, nullptr, "ReplyAndReceive1"},
    {0x4C, nullptr, "ReplyAndReceive2"},
    {0x4D, nullptr, "ReplyAndReceive3"},
    {0x4E, nullptr, "ReplyAndReceive4"},
    {0x4F, &SVC::Wrap<&SVC::ReplyAndReceive>, "ReplyAndReceive"},
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
    {0x7D, &SVC::Wrap<&SVC::QueryProcessMemory>, "QueryProcessMemory"},
};

const SVC::FunctionDef* SVC::GetSVCInfo(u32 func_num) {
    if (func_num >= ARRAY_SIZE(SVC_Table)) {
        LOG_ERROR(Kernel_SVC, "unknown svc=0x{:02X}", func_num);
        return nullptr;
    }
    return &SVC_Table[func_num];
}

MICROPROFILE_DEFINE(Kernel_SVC, "Kernel", "SVC", MP_RGB(70, 200, 70));

void SVC::CallSVC(u32 immediate) {
    MICROPROFILE_SCOPE(Kernel_SVC);

    // Lock the global kernel mutex when we enter the kernel HLE.
    std::lock_guard lock{HLE::g_hle_lock};

    DEBUG_ASSERT_MSG(kernel.GetCurrentProcess()->status == ProcessStatus::Running,
                     "Running threads from exiting processes is unimplemented");

    const FunctionDef* info = GetSVCInfo(immediate);
    if (info) {
        if (info->func) {
            (this->*(info->func))();
        } else {
            LOG_ERROR(Kernel_SVC, "unimplemented SVC function {}(..)", info->name);
        }
    }
}

SVC::SVC(Core::System& system) : system(system), kernel(system.Kernel()), memory(system.Memory()) {}

u32 SVC::GetReg(std::size_t n) {
    return system.GetRunningCore().GetReg(static_cast<int>(n));
}

void SVC::SetReg(std::size_t n, u32 value) {
    system.GetRunningCore().SetReg(static_cast<int>(n), value);
}

SVCContext::SVCContext(Core::System& system) : impl(std::make_unique<SVC>(system)) {}
SVCContext::~SVCContext() = default;

void SVCContext::CallSVC(u32 immediate) {
    impl->CallSVC(immediate);
}

} // namespace Kernel

SERIALIZE_EXPORT_IMPL(Kernel::SVC_SyncCallback)
SERIALIZE_EXPORT_IMPL(Kernel::SVC_IPCCallback)
