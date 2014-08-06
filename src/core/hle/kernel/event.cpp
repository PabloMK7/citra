// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include <map>
#include <algorithm>
#include <vector>

#include "common/common.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

class Event : public Object {
public:
    const char* GetTypeName() const { return "Event"; }
    const char* GetName() const { return name.c_str(); }

    static Kernel::HandleType GetStaticHandleType() { return Kernel::HandleType::Event; }
    Kernel::HandleType GetHandleType() const { return Kernel::HandleType::Event; }

    ResetType intitial_reset_type;          ///< ResetType specified at Event initialization
    ResetType reset_type;                   ///< Current ResetType

    bool locked;                            ///< Event signal wait
    bool permanent_locked;                  ///< Hack - to set event permanent state (for easy passthrough)
    std::vector<Handle> waiting_threads;    ///< Threads that are waiting for the event
    std::string name;                       ///< Name of event (optional)

    /**
     * Wait for kernel object to synchronize
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result WaitSynchronization(bool* wait) {
        *wait = locked;
        if (locked) {
            Handle thread = GetCurrentThreadHandle();
            if (std::find(waiting_threads.begin(), waiting_threads.end(), thread) == waiting_threads.end()) {
                waiting_threads.push_back(thread);
            }
            Kernel::WaitCurrentThread(WAITTYPE_EVENT, GetHandle());
        }
        if (reset_type != RESETTYPE_STICKY && !permanent_locked) {
            locked = true;
        }
        return 0;
    }
};

/**
 * Hackish function to set an events permanent lock state, used to pass through synch blocks
 * @param handle Handle to event to change
 * @param permanent_locked Boolean permanent locked value to set event
 * @return Result of operation, 0 on success, otherwise error code
 */
Result SetPermanentLock(Handle handle, const bool permanent_locked) {
    Event* evt = g_object_pool.GetFast<Event>(handle);
    _assert_msg_(KERNEL, (evt != nullptr), "called, but event is nullptr!");

    evt->permanent_locked = permanent_locked;
    return 0;
}

/**
 * Changes whether an event is locked or not
 * @param handle Handle to event to change
 * @param locked Boolean locked value to set event
 * @return Result of operation, 0 on success, otherwise error code
 */
Result SetEventLocked(const Handle handle, const bool locked) {
    Event* evt = g_object_pool.GetFast<Event>(handle);
    _assert_msg_(KERNEL, (evt != nullptr), "called, but event is nullptr!");

    if (!evt->permanent_locked) {
        evt->locked = locked;
    }
    return 0;
}

/**
 * Signals an event
 * @param handle Handle to event to signal
 * @return Result of operation, 0 on success, otherwise error code
 */
Result SignalEvent(const Handle handle) {
    Event* evt = g_object_pool.GetFast<Event>(handle);
    _assert_msg_(KERNEL, (evt != nullptr), "called, but event is nullptr!");

    // Resume threads waiting for event to signal
    bool event_caught = false;
    for (size_t i = 0; i < evt->waiting_threads.size(); ++i) {
        ResumeThreadFromWait( evt->waiting_threads[i]);

        // If any thread is signalled awake by this event, assume the event was "caught" and reset 
        // the event. This will result in the next thread waiting on the event to block. Otherwise,
        // the event will not be reset, and the next thread to call WaitSynchronization on it will
        // not block. Not sure if this is correct behavior, but it seems to work.
        event_caught = true;
    }
    evt->waiting_threads.clear();

    if (!evt->permanent_locked) {
        evt->locked = event_caught;
    }
    return 0;
}

/**
 * Clears an event
 * @param handle Handle to event to clear
 * @return Result of operation, 0 on success, otherwise error code
 */
Result ClearEvent(Handle handle) {
    Event* evt = g_object_pool.GetFast<Event>(handle);
    _assert_msg_(KERNEL, (evt != nullptr), "called, but event is nullptr!");

    if (!evt->permanent_locked) {
        evt->locked = true;
    }
    return 0;
}

/**
 * Creates an event
 * @param handle Reference to handle for the newly created mutex
 * @param reset_type ResetType describing how to create event
 * @param name Optional name of event
 * @return Newly created Event object
 */
Event* CreateEvent(Handle& handle, const ResetType reset_type, const std::string& name) {
    Event* evt = new Event;

    handle = Kernel::g_object_pool.Create(evt);

    evt->locked = true;
    evt->permanent_locked = false;
    evt->reset_type = evt->intitial_reset_type = reset_type;
    evt->name = name;

    return evt;
}

/**
 * Creates an event
 * @param reset_type ResetType describing how to create event
 * @param name Optional name of event
 * @return Handle to newly created Event object
 */
Handle CreateEvent(const ResetType reset_type, const std::string& name) {
    Handle handle;
    Event* evt = CreateEvent(handle, reset_type, name);
    return handle;
}

} // namespace
