// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <boost/smart_ptr/intrusive_ptr.hpp>
#include "common/common_types.h"
#include "core/hle/kernel/object.h"

namespace Kernel {

class Thread;

/// Class that represents a Kernel object that a thread can be waiting on
class WaitObject : public Object {
public:
    using Object::Object;

    /**
     * Check if the specified thread should wait until the object is available
     * @param thread The thread about which we're deciding.
     * @return True if the current thread should wait due to this object being unavailable
     */
    virtual bool ShouldWait(Thread* thread) const = 0;

    /// Acquire/lock the object for the specified thread if it is available
    virtual void Acquire(Thread* thread) = 0;

    /**
     * Add a thread to wait on this object
     * @param thread Pointer to thread to add
     */
    virtual void AddWaitingThread(SharedPtr<Thread> thread);

    /**
     * Removes a thread from waiting on this object (e.g. if it was resumed already)
     * @param thread Pointer to thread to remove
     */
    virtual void RemoveWaitingThread(Thread* thread);

    /**
     * Wake up all threads waiting on this object that can be awoken, in priority order,
     * and set the synchronization result and output of the thread.
     */
    virtual void WakeupAllWaitingThreads();

    /// Obtains the highest priority thread that is ready to run from this object's waiting list.
    SharedPtr<Thread> GetHighestPriorityReadyThread();

    /// Get a const reference to the waiting threads list for debug use
    const std::vector<SharedPtr<Thread>>& GetWaitingThreads() const;

private:
    /// Threads waiting for this object to become available
    std::vector<SharedPtr<Thread>> waiting_threads;
};

// Specialization of DynamicObjectCast for WaitObjects
template <>
inline SharedPtr<WaitObject> DynamicObjectCast<WaitObject>(SharedPtr<Object> object) {
    if (object != nullptr && object->IsWaitable()) {
        return boost::static_pointer_cast<WaitObject>(object);
    }
    return nullptr;
}

} // namespace Kernel
