// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include "common/common_types.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/wait_object.h"
#include "core/hle/result.h"

namespace Kernel {

class Thread;

class Mutex final : public WaitObject {
public:
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

    /**
     * Elevate the mutex priority to the best priority
     * among the priorities of all its waiting threads.
     */
    void UpdatePriority();

    bool ShouldWait(Thread* thread) const override;
    void Acquire(Thread* thread) override;

    void AddWaitingThread(SharedPtr<Thread> thread) override;
    void RemoveWaitingThread(Thread* thread) override;

    /**
     * Attempts to release the mutex from the specified thread.
     * @param thread Thread that wants to release the mutex.
     * @returns The result code of the operation.
     */
    ResultCode Release(Thread* thread);

private:
    explicit Mutex(KernelSystem& kernel);
    ~Mutex() override;

    friend class KernelSystem;
};

/**
 * Releases all the mutexes held by the specified thread
 * @param thread Thread that is holding the mutexes
 */
void ReleaseThreadMutexes(Thread* thread);

} // namespace Kernel
