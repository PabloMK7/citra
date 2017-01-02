// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"

namespace Kernel {

class Thread;

class Mutex final : public WaitObject {
public:
    /**
     * Creates a mutex.
     * @param initial_locked Specifies if the mutex should be locked initially
     * @param name Optional name of mutex
     * @return Pointer to new Mutex object
     */
    static SharedPtr<Mutex> Create(bool initial_locked, std::string name = "Unknown");

    std::string GetTypeName() const override {
        return "Mutex";
    }
    std::string GetName() const override {
        return name;
    }

    static const HandleType HANDLE_TYPE = HandleType::Mutex;
    HandleType GetHandleType() const override {
        return HANDLE_TYPE;
    }

    int lock_count;                   ///< Number of times the mutex has been acquired
    u32 priority;                     ///< The priority of the mutex, used for priority inheritance.
    std::string name;                 ///< Name of mutex (optional)
    SharedPtr<Thread> holding_thread; ///< Thread that has acquired the mutex

    bool ShouldWait(Thread* thread) const override;
    void Acquire(Thread* thread) override;

    void AddWaitingThread(SharedPtr<Thread> thread) override;
    void RemoveWaitingThread(Thread* thread) override;

    void Release();

private:
    Mutex();
    ~Mutex() override;
};

/**
 * Releases all the mutexes held by the specified thread
 * @param thread Thread that is holding the mutexes
 */
void ReleaseThreadMutexes(Thread* thread);

} // namespace
