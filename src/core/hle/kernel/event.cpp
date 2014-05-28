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

    /**
     * Synchronize kernel object 
     * @param wait Boolean wait set if current thread should wait as a result of sync operation
     * @return Result of operation, 0 on success, otherwise error code
     */
    Result SyncRequest(bool* wait) {
        // TODO(bunnei): ImplementMe
        ERROR_LOG(KERNEL, "Unimplemented function Event::SyncRequest");
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
        if (reset_type != RESETTYPE_STICKY) {
            locked = true;
        }
        return 0;
    }
};

/**
 * Changes whether an event is locked or not
 * @param handle Handle to event to change
 * @param locked Boolean locked value to set event
 */
void SetEventLocked(const Handle handle, const bool locked) {
    Event* evt = g_object_pool.GetFast<Event>(handle);
    evt->locked = locked;
    return;
}

/**
 * Clears an event
 * @param handle Handle to event to clear
 * @return Result of operation, 0 on success, otherwise error code
 */
Result ClearEvent(Handle handle) {
    ERROR_LOG(KERNEL, "Unimplemented function ClearEvent");
    return 0;
}

/**
 * Creates an event
 * @param handle Reference to handle for the newly created mutex
 * @param reset_type ResetType describing how to create event
 * @return Handle to newly created object
 */
Event* CreateEvent(Handle& handle, const ResetType reset_type) {
    Event* evt = new Event;

    handle = Kernel::g_object_pool.Create(evt);

    evt->reset_type = evt->intitial_reset_type = reset_type;
    evt->locked = false;

    return evt;
}

/**
 * Creates an event
 * @param reset_type ResetType describing how to create event
 * @return Handle to newly created object
 */
Handle CreateEvent(const ResetType reset_type) {
    Handle handle;
    Event* evt = CreateEvent(handle, reset_type);
    return handle;
}

} // namespace
