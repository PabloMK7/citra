// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <memory>
#include <string>
#include <vector>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include "common/common_types.h"
#include "core/hle/result.h"

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

template <typename T>
using SharedPtr = boost::intrusive_ptr<T>;

class KernelSystem {
public:
    explicit KernelSystem(u32 system_mode);
    ~KernelSystem();

    /**
     * Creates an address arbiter.
     *
     * @param name Optional name used for debugging.
     * @returns The created AddressArbiter.
     */
    SharedPtr<AddressArbiter> CreateAddressArbiter(std::string name = "Unknown");

    /**
     * Creates an event
     * @param reset_type ResetType describing how to create event
     * @param name Optional name of event
     */
    SharedPtr<Event> CreateEvent(ResetType reset_type, std::string name = "Unknown");

    /**
     * Creates a mutex.
     * @param initial_locked Specifies if the mutex should be locked initially
     * @param name Optional name of mutex
     * @return Pointer to new Mutex object
     */
    SharedPtr<Mutex> CreateMutex(bool initial_locked, std::string name = "Unknown");

    SharedPtr<CodeSet> CreateCodeSet(std::string name, u64 program_id);

    SharedPtr<Process> CreateProcess(SharedPtr<CodeSet> code_set);

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
    ResultVal<SharedPtr<Thread>> CreateThread(std::string name, VAddr entry_point, u32 priority,
                                              u32 arg, s32 processor_id, VAddr stack_top,
                                              Process& owner_process);

    /**
     * Creates a semaphore.
     * @param initial_count Number of slots reserved for other threads
     * @param max_count Maximum number of slots the semaphore can have
     * @param name Optional name of semaphore
     * @return The created semaphore
     */
    ResultVal<SharedPtr<Semaphore>> CreateSemaphore(s32 initial_count, s32 max_count,
                                                    std::string name = "Unknown");

    /**
     * Creates a timer
     * @param reset_type ResetType describing how to create the timer
     * @param name Optional name of timer
     * @return The created Timer
     */
    SharedPtr<Timer> CreateTimer(ResetType reset_type, std::string name = "Unknown");

    /**
     * Creates a pair of ServerPort and an associated ClientPort.
     *
     * @param max_sessions Maximum number of sessions to the port
     * @param name Optional name of the ports
     * @return The created port tuple
     */
    std::tuple<SharedPtr<ServerPort>, SharedPtr<ClientPort>> CreatePortPair(
        u32 max_sessions, std::string name = "UnknownPort");

    /**
     * Creates a pair of ServerSession and an associated ClientSession.
     * @param name        Optional name of the ports.
     * @param client_port Optional The ClientPort that spawned this session.
     * @return The created session tuple
     */
    std::tuple<SharedPtr<ServerSession>, SharedPtr<ClientSession>> CreateSessionPair(
        const std::string& name = "Unknown", SharedPtr<ClientPort> client_port = nullptr);

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
    SharedPtr<SharedMemory> CreateSharedMemory(Process* owner_process, u32 size,
                                               MemoryPermission permissions,
                                               MemoryPermission other_permissions,
                                               VAddr address = 0,
                                               MemoryRegion region = MemoryRegion::BASE,
                                               std::string name = "Unknown");

    /**
     * Creates a shared memory object from a block of memory managed by an HLE applet.
     * @param heap_block Heap block of the HLE applet.
     * @param offset The offset into the heap block that the SharedMemory will map.
     * @param size Size of the memory block. Must be page-aligned.
     * @param permissions Permission restrictions applied to the process which created the block.
     * @param other_permissions Permission restrictions applied to other processes mapping the
     * block.
     * @param name Optional object name, used for debugging purposes.
     */
    SharedPtr<SharedMemory> CreateSharedMemoryForApplet(std::shared_ptr<std::vector<u8>> heap_block,
                                                        u32 offset, u32 size,
                                                        MemoryPermission permissions,
                                                        MemoryPermission other_permissions,
                                                        std::string name = "Unknown Applet");

    u32 GenerateObjectID();

    /// Retrieves a process from the current list of processes.
    SharedPtr<Process> GetProcessById(u32 process_id) const;

    SharedPtr<Process> GetCurrentProcess() const;
    void SetCurrentProcess(SharedPtr<Process> process);

private:
    std::unique_ptr<ResourceLimitList> resource_limits;
    std::atomic<u32> next_object_id{0};

    // TODO(Subv): Start the process ids from 10 for now, as lower PIDs are
    // reserved for low-level services
    u32 next_process_id = 10;

    // Lists all processes that exist in the current session.
    std::vector<SharedPtr<Process>> process_list;

    SharedPtr<Process> current_process;
};

} // namespace Kernel
