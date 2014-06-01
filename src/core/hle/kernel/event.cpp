// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.  

#include <map>
#include <vector>

#include "common/common.h"

#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/event.h"

namespace Kernel {

class Event : public Object {
public:
    const char* GetTypeName() { return "Event"; }

    static Kernel::HandleType GetStaticHandleType() {  return Kernel::HandleType::Event; }
    Kernel::HandleType GetHandleType() const { return Kernel::HandleType::Event; }

    ResetType intitial_reset_type;  ///< ResetType specified at Event initialization
    ResetType reset_type;           ///< Current ResetType

    bool locked;                    ///< Current locked state
    bool permanent_locked;          ///< Hack - to set event permanent state (for easy passthrough)

    /**
     * Synchronize kernel object 
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result SyncRequest(bool* wait) {
        // TODO(bunnei): ImplementMe
        ERROR_LOG(KERNEL, "(UMIMPLEMENTED) call");
        return 0;
    }

    /**
     * Wait for kernel object to synchronize
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result WaitSynchronization(bool* wait) {
        // TODO(bunnei): ImplementMe
        *wait = locked;
        if (reset_type != RESETTYPE_STICKY && !permanent_locked) {
            locked = true;
        }
        return 0;
    }
};

/**
 * Changes whether an event is locked or not
 * @param handle Handle to event to change
 * @param locked Boolean locked value to set event
 * @return Result of operation, 0 on success, otherwise error code
 */
Result SetEventLocked(const Handle handle, const bool locked) {
    Event* evt = g_object_pool.GetFast<Event>(handle);
    if (!evt) {
        ERROR_LOG(KERNEL, "called with unknown handle=0x%08X", handle);
        return -1;
    }
    if (!evt->permanent_locked) {
        evt->locked = locked;
    }
    return 0;
}

/**
 * Hackish function to set an events permanent lock state, used to pass through synch blocks
 * @param handle Handle to event to change
 * @param permanent_locked Boolean permanent locked value to set event
 * @return Result of operation, 0 on success, otherwise error code
 */
Result SetPermanentLock(Handle handle, const bool permanent_locked) {
    Event* evt = g_object_pool.GetFast<Event>(handle);
    if (!evt) {
        ERROR_LOG(KERNEL, "called with unknown handle=0x%08X", handle);
        return -1;
    }
    evt->permanent_locked = permanent_locked;
    return 0;
}

/**
 * Clears an event
 * @param handle Handle to event to clear
 * @return Result of operation, 0 on success, otherwise error code
 */
Result ClearEvent(Handle handle) {
    return SetEventLocked(handle, true);
}

/**
 * Creates an event
 * @param handle Reference to handle for the newly created mutex
 * @param reset_type ResetType describing how to create event
 * @return Newly created Event object
 */
Event* CreateEvent(Handle& handle, const ResetType reset_type) {
    Event* evt = new Event;

    handle = Kernel::g_object_pool.Create(evt);

    evt->locked = true;
    evt->permanent_locked = false;
    evt->reset_type = evt->intitial_reset_type = reset_type;

    return evt;
}

/**
 * Creates an event
 * @param reset_type ResetType describing how to create event
 * @return Handle to newly created Event object
 */
Handle CreateEvent(const ResetType reset_type) {
    Handle handle;
    Event* evt = CreateEvent(handle, reset_type);
    return handle;
}

} // namespace
