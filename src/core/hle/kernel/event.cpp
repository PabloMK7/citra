// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <map>
#include <vector>
#include "common/assert.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

Event::Event(KernelSystem& kernel) : WaitObject(kernel) {}
Event::~Event() {}

SharedPtr<Event> KernelSystem::CreateEvent(ResetType reset_type, std::string name) {
    SharedPtr<Event> evt(new Event(*this));

    evt->signaled = false;
    evt->reset_type = reset_type;
    evt->name = std::move(name);

    return evt;
}

bool Event::ShouldWait(Thread* thread) const {
    return !signaled;
}

void Event::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");

    if (reset_type == ResetType::OneShot)
        signaled = false;
}

void Event::Signal() {
    signaled = true;
    WakeupAllWaitingThreads();
}

void Event::Clear() {
    signaled = false;
}

void Event::WakeupAllWaitingThreads() {
    WaitObject::WakeupAllWaitingThreads();

    if (reset_type == ResetType::Pulse)
        signaled = false;
}

} // namespace Kernel
