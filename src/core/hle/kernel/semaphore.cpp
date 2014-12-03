// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <map>
#include <vector>

#include "common/common.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/semaphore.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

class Semaphore : public Object {
public:
    std::string GetTypeName() const override { return "Semaphore"; }
    std::string GetName() const override { return name; }

    static Kernel::HandleType GetStaticHandleType() { return Kernel::HandleType::Semaphore; }
    Kernel::HandleType GetHandleType() const override { return Kernel::HandleType::Semaphore; }

    u32 initial_count;                          ///< Number of reserved entries
    u32 max_count;                              ///< Maximum number of simultaneous holders the semaphore can have
    u32 current_usage;                          ///< Number of currently used entries in the semaphore
    std::vector<Handle> waiting_threads;        ///< Threads that are waiting for the semaphore
    std::string name;                           ///< Name of semaphore (optional)

    ResultVal<bool> SyncRequest() override {
        // TODO(Subv): ImplementMe
        return MakeResult<bool>(false);
    }

    ResultVal<bool> WaitSynchronization() override {
        bool wait = current_usage == max_count;

        if (wait) {
            Kernel::WaitCurrentThread(WAITTYPE_SEMA, GetHandle());
            waiting_threads.push_back(GetCurrentThreadHandle());
        } else {
            ++current_usage;
        }

        return MakeResult<bool>(wait);
    }
};

////////////////////////////////////////////////////////////////////////////////////////////////////

/**
 * Creates a semaphore
 * @param handle Reference to handle for the newly created semaphore
 * @param initial_count initial amount of times the semaphore is held
 * @param max_count maximum number of holders the semaphore can have
 * @param name Optional name of semaphore
 * @return Pointer to new Semaphore object
 */
Semaphore* CreateSemaphore(Handle& handle, u32 initial_count, u32 max_count, const std::string& name) {
    Semaphore* semaphore = new Semaphore;
    handle = Kernel::g_object_pool.Create(semaphore);

    semaphore->initial_count = semaphore->current_usage = initial_count;
    semaphore->max_count = max_count;
    semaphore->name = name;

    return semaphore;
}

Handle CreateSemaphore(u32 initial_count, u32 max_count, const std::string& name) {
    Handle handle;
    Semaphore* semaphore = CreateSemaphore(handle, initial_count, max_count, name);
    return handle;
}

} // namespace
