// Copyright 2008 Dolphin Emulator Project / 2017 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <algorithm>
#include <cinttypes>
#include <tuple>
#include "common/assert.h"
#include "common/logging/log.h"
#include "core/core_timing.h"

namespace Core {

// Sort by time, unless the times are the same, in which case sort by the order added to the queue
bool Timing::Event::operator>(const Event& right) const {
    return std::tie(time, fifo_order) > std::tie(right.time, right.fifo_order);
}

bool Timing::Event::operator<(const Event& right) const {
    return std::tie(time, fifo_order) < std::tie(right.time, right.fifo_order);
}

TimingEventType* Timing::RegisterEvent(const std::string& name, TimedCallback callback) {
    // check for existing type with same name.
    // we want event type names to remain unique so that we can use them for serialization.
    ASSERT_MSG(event_types.find(name) == event_types.end(),
               "CoreTiming Event \"{}\" is already registered. Events should only be registered "
               "during Init to avoid breaking save states.",
               name);

    auto info = event_types.emplace(name, TimingEventType{callback, nullptr});
    TimingEventType* event_type = &info.first->second;
    event_type->name = &info.first->first;
    return event_type;
}

Timing::~Timing() {
    MoveEvents();
}

u64 Timing::GetTicks() const {
    u64 ticks = static_cast<u64>(global_timer);
    if (!is_global_timer_sane) {
        ticks += slice_length - downcount;
    }
    return ticks;
}

void Timing::AddTicks(u64 ticks) {
    downcount -= ticks;
}

u64 Timing::GetIdleTicks() const {
    return static_cast<u64>(idled_cycles);
}

void Timing::ScheduleEvent(s64 cycles_into_future, const TimingEventType* event_type,
                           u64 userdata) {
    ASSERT(event_type != nullptr);
    s64 timeout = GetTicks() + cycles_into_future;

    // If this event needs to be scheduled before the next advance(), force one early
    if (!is_global_timer_sane)
        ForceExceptionCheck(cycles_into_future);

    event_queue.emplace_back(Event{timeout, event_fifo_id++, userdata, event_type});
    std::push_heap(event_queue.begin(), event_queue.end(), std::greater<>());
}

void Timing::ScheduleEventThreadsafe(s64 cycles_into_future, const TimingEventType* event_type,
                                     u64 userdata) {
    ts_queue.Push(Event{global_timer + cycles_into_future, 0, userdata, event_type});
}

void Timing::UnscheduleEvent(const TimingEventType* event_type, u64 userdata) {
    auto itr = std::remove_if(event_queue.begin(), event_queue.end(), [&](const Event& e) {
        return e.type == event_type && e.userdata == userdata;
    });

    // Removing random items breaks the invariant so we have to re-establish it.
    if (itr != event_queue.end()) {
        event_queue.erase(itr, event_queue.end());
        std::make_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }
}

void Timing::RemoveEvent(const TimingEventType* event_type) {
    auto itr = std::remove_if(event_queue.begin(), event_queue.end(),
                              [&](const Event& e) { return e.type == event_type; });

    // Removing random items breaks the invariant so we have to re-establish it.
    if (itr != event_queue.end()) {
        event_queue.erase(itr, event_queue.end());
        std::make_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }
}

void Timing::RemoveNormalAndThreadsafeEvent(const TimingEventType* event_type) {
    MoveEvents();
    RemoveEvent(event_type);
}

void Timing::ForceExceptionCheck(s64 cycles) {
    cycles = std::max<s64>(0, cycles);
    if (downcount > cycles) {
        slice_length -= downcount - cycles;
        downcount = cycles;
    }
}

void Timing::MoveEvents() {
    for (Event ev; ts_queue.Pop(ev);) {
        ev.fifo_order = event_fifo_id++;
        event_queue.emplace_back(std::move(ev));
        std::push_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }
}

void Timing::Advance() {
    MoveEvents();

    s64 cycles_executed = slice_length - downcount;
    global_timer += cycles_executed;
    slice_length = MAX_SLICE_LENGTH;

    is_global_timer_sane = true;

    while (!event_queue.empty() && event_queue.front().time <= global_timer) {
        Event evt = std::move(event_queue.front());
        std::pop_heap(event_queue.begin(), event_queue.end(), std::greater<>());
        event_queue.pop_back();
        evt.type->callback(evt.userdata, global_timer - evt.time);
    }

    is_global_timer_sane = false;

    // Still events left (scheduled in the future)
    if (!event_queue.empty()) {
        slice_length = static_cast<int>(
            std::min<s64>(event_queue.front().time - global_timer, MAX_SLICE_LENGTH));
    }

    downcount = slice_length;
}

void Timing::Idle() {
    idled_cycles += downcount;
    downcount = 0;
}

std::chrono::microseconds Timing::GetGlobalTimeUs() const {
    return std::chrono::microseconds{GetTicks() * 1000000 / BASE_CLOCK_RATE_ARM11};
}

s64 Timing::GetDowncount() const {
    return downcount;
}

} // namespace Core
