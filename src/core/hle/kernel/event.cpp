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

class Event : public WaitObject {
public:
    std::string GetTypeName() const override { return "Event"; }
    std::string GetName() const override { return name; }

    static const HandleType HANDLE_TYPE = HandleType::Event;
    HandleType GetHandleType() const override { return HANDLE_TYPE; }

    ResetType intitial_reset_type;          ///< ResetType specified at Event initialization
    ResetType reset_type;                   ///< Current ResetType

    bool signaled;                          ///< Whether the event has already been signaled
    std::string name;                       ///< Name of event (optional)

    ResultVal<bool> Wait() override {
        return MakeResult<bool>(!signaled);
    }

    ResultVal<bool> Acquire() override {
        // Release the event if it's not sticky...
        if (reset_type != RESETTYPE_STICKY)
            signaled = false;

        return MakeResult<bool>(true);
    }
};

ResultCode SignalEvent(const Handle handle) {
    Event* evt = g_handle_table.Get<Event>(handle).get();
    if (evt == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    evt->signaled = true;
    evt->ReleaseAllWaitingThreads();

    return RESULT_SUCCESS;
}

ResultCode ClearEvent(Handle handle) {
    Event* evt = g_handle_table.Get<Event>(handle).get();
    if (evt == nullptr)
        return InvalidHandle(ErrorModule::Kernel);

    evt->signaled = false;

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

    // TOOD(yuriks): Fix error reporting
    handle = Kernel::g_handle_table.Create(evt).ValueOr(INVALID_HANDLE);

    evt->signaled = false;
    evt->reset_type = evt->intitial_reset_type = reset_type;
    evt->name = name;

    return evt;
}

Handle CreateEvent(const ResetType reset_type, const std::string& name) {
    Handle handle;
    Event* evt = CreateEvent(handle, reset_type, name);
    return handle;
}

} // namespace
