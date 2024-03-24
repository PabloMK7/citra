// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/assert.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/kernel/resource_limit.h"
#include "core/hle/kernel/thread.h"

namespace Kernel {

Event::Event(KernelSystem& kernel) : WaitObject(kernel) {}

Event::~Event() {
    if (resource_limit) {
        resource_limit->Release(ResourceLimitType::Event, 1);
    }
}

std::shared_ptr<Event> KernelSystem::CreateEvent(ResetType reset_type, std::string name) {
    auto event = std::make_shared<Event>(*this);
    event->signaled = false;
    event->reset_type = reset_type;
    event->name = std::move(name);
    return event;
}

bool Event::ShouldWait(const Thread* thread) const {
    return !signaled;
}

void Event::Acquire(Thread* thread) {
    ASSERT_MSG(!ShouldWait(thread), "object unavailable!");

    if (reset_type == ResetType::OneShot) {
        signaled = false;
    }
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

    if (reset_type == ResetType::Pulse) {
        signaled = false;
    }
}

} // namespace Kernel
