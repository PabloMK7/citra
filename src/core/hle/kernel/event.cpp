// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
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
    std::string GetTypeName() const override { return "Event"; }
    std::string GetName() const override { return name; }

    static Kernel::HandleType GetStaticHandleType() { return Kernel::HandleType::Event; }
    Kernel::HandleType GetHandleType() const override { return Kernel::HandleType::Event; }

    ResetType intitial_reset_type;          ///< ResetType specified at Event initialization
    ResetType reset_type;                   ///< Current ResetType

    bool locked;                            ///< Event signal wait
    bool permanent_locked;                  ///< Hack - to set event permanent state (for easy passthrough)
    std::vector<Handle> waiting_threads;    ///< Threads that are waiting for the event
    std::string name;                       ///< Name of event (optional)

    ResultVal<bool> WaitSynchronization() override {
        bool wait = locked;
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
        return MakeResult<bool>(wait);
    }
};

/**
 * Hackish function to set an events permanent lock state, used to pass through synch blocks
 * @param handle Handle to event to change
 * @param permanent_locked Boolean permanent locked value to set event
 * @return Result of operation, 0 on success, otherwise error code
 */
ResultCode SetPermanentLock(Handle handle, const bool permanent_locked) {
    Event* evt = g_object_pool.Get<Event>(handle);
    if (evt == nullptr) return InvalidHandle(ErrorModule::Kernel);

    evt->permanent_locked = permanent_locked;
    return RESULT_SUCCESS;
}

/**
 * Changes whether an event is locked or not
 * @param handle Handle to event to change
 * @param locked Boolean locked value to set event
 * @return Result of operation, 0 on success, otherwise error code
 */
ResultCode SetEventLocked(const Handle handle, const bool locked) {
    Event* evt = g_object_pool.Get<Event>(handle);
    if (evt == nullptr) return InvalidHandle(ErrorModule::Kernel);

    if (!evt->permanent_locked) {
        evt->locked = locked;
    }
    return RESULT_SUCCESS;
}

/**
 * Signals an event
 * @param handle Handle to event to signal
 * @return Result of operation, 0 on success, otherwise error code
 */
ResultCode SignalEvent(const Handle handle) {
    Event* evt = g_object_pool.Get<Event>(handle);
    if (evt == nullptr) return InvalidHandle(ErrorModule::Kernel);

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
    return RESULT_SUCCESS;
}

/**
 * Clears an event
 * @param handle Handle to event to clear
 * @return Result of operation, 0 on success, otherwise error code
 */
ResultCode ClearEvent(Handle handle) {
    Event* evt = g_object_pool.Get<Event>(handle);
    if (evt == nullptr) return InvalidHandle(ErrorModule::Kernel);

    if (!evt->permanent_locked) {
        evt->locked = true;
    }
    return RESULT_SUCCESS;
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
