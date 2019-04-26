// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <atomic>
#include <functional>
#include <memory>
#include <string>
#include <unordered_map>
#include <vector>
#include "common/common_types.h"
#include "core/hle/kernel/memory.h"
#include "core/hle/result.h"

namespace ConfigMem {
class Handler;
}

namespace SharedPage {
class Handler;
}

namespace Memory {
class MemorySystem;
}

namespace Core {
class Timing;
}

namespace Kernel {

class AddressArbiter;
class Event;
class Mutex;
class CodeSet;
class Process;
class Thread;
class Semaphore;
class Timer;
class ClientPort;
class ServerPort;
class ClientSession;
class ServerSession;
class ResourceLimitList;
class SharedMemory;
class ThreadManager;
class TimerManager;
class VMManager;
struct AddressMapping;

enum class ResetType {
    OneShot,
    Sticky,
    Pulse,
};

/// Permissions for mapped shared memory blocks
enum class MemoryPermission : u32 {
    None = 0,
    Read = (1u << 0),
    Write = (1u << 1),
    ReadWrite = (Read | Write),
    Execute = (1u << 2),
    ReadExecute = (Read | Execute),
    WriteExecute = (Write | Execute),
    ReadWriteExecute = (Read | Write | Execute),
    DontCare = (1u << 28)
};

enum class MemoryRegion : u16 {
    APPLICATION = 1,
    SYSTEM = 2,
    BASE = 3,
};

class KernelSystem {
public:
    explicit KernelSystem(Memory::MemorySystem& memory, Core::Timing& timing,
                          std::function<void()> prepare_reschedule_callback, u32 system_mode);
    ~KernelSystem();

    using PortPair = std::pair<std::shared_ptr<ServerPort>, std::shared_ptr<ClientPort>>;
    using SessionPair = std::pair<std::shared_ptr<ServerSession>, std::shared_ptr<ClientSession>>;

    /**
     * Creates an address arbiter.
     *
     * @param name Optional name used for debugging.
     * @returns The created AddressArbiter.
     */
    std::shared_ptr<AddressArbiter> CreateAddressArbiter(std::string name = "Unknown");

    /**
     * Creates an event
     * @param reset_type ResetType describing how to create event
     * @param name Optional name of event
     */
    std::shared_ptr<Event> CreateEvent(ResetType reset_type, std::string name = "Unknown");

    /**
     * Creates a mutex.
     * @param initial_locked Specifies if the mutex should be locked initially
     * @param name Optional name of mutex
     * @return Pointer to new Mutex object
     */
    std::shared_ptr<Mutex> CreateMutex(bool initial_locked, std::string name = "Unknown");

    std::shared_ptr<CodeSet> CreateCodeSet(std::string name, u64 program_id);

    std::shared_ptr<Process> CreateProcess(std::shared_ptr<CodeSet> code_set);

    /**
     * Creates and returns a new thread. The new thread is immediately scheduled
     * @param name The friendly name desired for the thread
     * @param entry_point The address at which the thread should start execution
     * @param priority The thread's priority
     * @param arg User data to pass to the thread
     * @param processor_id The ID(s) of the processors on which the thread is desired to be run
     * @param stack_top The address of the thread's stack top
     * @param owner_process The parent process for the thread
     * @return A shared pointer to the newly created thread
     */
    ResultVal<std::shared_ptr<Thread>> CreateThread(std::string name, VAddr entry_point,
                                                    u32 priority, u32 arg, s32 processor_id,
                                                    VAddr stack_top, Process& owner_process);

    /**
     * Creates a semaphore.
     * @param initial_count Number of slots reserved for other threads
     * @param max_count Maximum number of slots the semaphore can have
     * @param name Optional name of semaphore
     * @return The created semaphore
     */
    ResultVal<std::shared_ptr<Semaphore>> CreateSemaphore(s32 initial_count, s32 max_count,
                                                          std::string name = "Unknown");

    /**
     * Creates a timer
     * @param reset_type ResetType describing how to create the timer
     * @param name Optional name of timer
     * @return The created Timer
     */
    std::shared_ptr<Timer> CreateTimer(ResetType reset_type, std::string name = "Unknown");

    /**
     * Creates a pair of ServerPort and an associated ClientPort.
     *
     * @param max_sessions Maximum number of sessions to the port
     * @param name Optional name of the ports
     * @return The created port tuple
     */
    PortPair CreatePortPair(u32 max_sessions, std::string name = "UnknownPort");

    /**
     * Creates a pair of ServerSession and an associated ClientSession.
     * @param name        Optional name of the ports.
     * @param client_port Optional The ClientPort that spawned this session.
     * @return The created session tuple
     */
    SessionPair CreateSessionPair(const std::string& name = "Unknown",
                                  std::shared_ptr<ClientPort> client_port = nullptr);

    ResourceLimitList& ResourceLimit();
    const ResourceLimitList& ResourceLimit() const;

    /**
     * Creates a shared memory object.
     * @param owner_process Process that created this shared memory object.
     * @param size Size of the memory block. Must be page-aligned.
     * @param permissions Permission restrictions applied to the process which created the block.
     * @param other_permissions Permission restrictions applied to other processes mapping the
     * block.
     * @param address The address from which to map the Shared Memory.
     * @param region If the address is 0, the shared memory will be allocated in this region of the
     * linear heap.
     * @param name Optional object name, used for debugging purposes.
     */
    ResultVal<std::shared_ptr<SharedMemory>> CreateSharedMemory(
        Process* owner_process, u32 size, MemoryPermission permissions,
        MemoryPermission other_permissions, VAddr address = 0,
        MemoryRegion region = MemoryRegion::BASE, std::string name = "Unknown");

    /**
     * Creates a shared memory object from a block of memory managed by an HLE applet.
     * @param offset The offset into the heap block that the SharedMemory will map.
     * @param size Size of the memory block. Must be page-aligned.
     * @param permissions Permission restrictions applied to the process which created the block.
     * @param other_permissions Permission restrictions applied to other processes mapping the
     * block.
     * @param name Optional object name, used for debugging purposes.
     */
    std::shared_ptr<SharedMemory> CreateSharedMemoryForApplet(u32 offset, u32 size,
                                                              MemoryPermission permissions,
                                                              MemoryPermission other_permissions,
                                                              std::string name = "Unknown Applet");

    u32 GenerateObjectID();

    /// Retrieves a process from the current list of processes.
    std::shared_ptr<Process> GetProcessById(u32 process_id) const;

    std::shared_ptr<Process> GetCurrentProcess() const;
    void SetCurrentProcess(std::shared_ptr<Process> process);

    ThreadManager& GetThreadManager();
    const ThreadManager& GetThreadManager() const;

    TimerManager& GetTimerManager();
    const TimerManager& GetTimerManager() const;

    void MapSharedPages(VMManager& address_space);

    SharedPage::Handler& GetSharedPageHandler();
    const SharedPage::Handler& GetSharedPageHandler() const;

    MemoryRegionInfo* GetMemoryRegion(MemoryRegion region);

    void HandleSpecialMapping(VMManager& address_space, const AddressMapping& mapping);

    std::array<MemoryRegionInfo, 3> memory_regions;

    /// Adds a port to the named port table
    void AddNamedPort(std::string name, std::shared_ptr<ClientPort> port);

    void PrepareReschedule() {
        prepare_reschedule_callback();
    }

    /// Map of named ports managed by the kernel, which can be retrieved using the ConnectToPort
    std::unordered_map<std::string, std::shared_ptr<ClientPort>> named_ports;

    Memory::MemorySystem& memory;

    Core::Timing& timing;

private:
    void MemoryInit(u32 mem_type);

    std::function<void()> prepare_reschedule_callback;

    std::unique_ptr<ResourceLimitList> resource_limits;
    std::atomic<u32> next_object_id{0};

    // Note: keep the member order below in order to perform correct destruction.
    // Thread manager is destructed before process list in order to Stop threads and clear thread
    // info from their parent processes first. Timer manager is destructed after process list
    // because timers are destructed along with process list and they need to clear info from the
    // timer manager.
    // TODO (wwylele): refactor the cleanup sequence to make this less complicated and sensitive.

    std::unique_ptr<TimerManager> timer_manager;

    // TODO(Subv): Start the process ids from 10 for now, as lower PIDs are
    // reserved for low-level services
    u32 next_process_id = 10;

    // Lists all processes that exist in the current session.
    std::vector<std::shared_ptr<Process>> process_list;

    std::shared_ptr<Process> current_process;

    std::unique_ptr<ThreadManager> thread_manager;

    std::unique_ptr<ConfigMem::Handler> config_mem_handler;
    std::unique_ptr<SharedPage::Handler> shared_page_handler;
};

} // namespace Kernel
