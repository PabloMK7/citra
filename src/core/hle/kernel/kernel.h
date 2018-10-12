// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
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

enum class ResetType {
    OneShot,
    Sticky,
    Pulse,
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
                                              SharedPtr<Process> owner_process);

    /**
     * Creates a semaphore.
     * @param initial_count Number of slots reserved for other threads
     * @param max_count Maximum number of slots the semaphore can have
     * @param name Optional name of semaphore
     * @return The created semaphore
     */
    ResultVal<SharedPtr<Semaphore>> CreateSemaphore(s32 initial_count, s32 max_count,
                                                    std::string name = "Unknown");
};

} // namespace Kernel
