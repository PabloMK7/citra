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
bool Timing::Event::operator>(const Timing::Event& right) const {
    return std::tie(time, fifo_order) > std::tie(right.time, right.fifo_order);
}

bool Timing::Event::operator<(const Timing::Event& right) const {
    return std::tie(time, fifo_order) < std::tie(right.time, right.fifo_order);
}

Timing::Timing(std::size_t num_cores, u32 cpu_clock_percentage) {
    timers.resize(num_cores);
    for (std::size_t i = 0; i < num_cores; ++i) {
        timers[i] = std::make_shared<Timer>(100.0 / cpu_clock_percentage);
    }
    current_timer = timers[0];
}

void Timing::UpdateClockSpeed(u32 cpu_clock_percentage) {
    for (auto& timer : timers) {
        timer->cpu_clock_scale = 100.0 / cpu_clock_percentage;
    }
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

void Timing::ScheduleEvent(s64 cycles_into_future, const TimingEventType* event_type, u64 userdata,
                           std::size_t core_id) {
    ASSERT(event_type != nullptr);
    std::shared_ptr<Timing::Timer> timer;
    if (core_id == std::numeric_limits<std::size_t>::max()) {
        timer = current_timer;
    } else {
        ASSERT(core_id < timers.size());
        timer = timers.at(core_id);
    }

    s64 timeout = timer->GetTicks() + cycles_into_future;
    if (current_timer == timer) {
        // If this event needs to be scheduled before the next advance(), force one early
        if (!timer->is_timer_sane)
            timer->ForceExceptionCheck(cycles_into_future);

        timer->event_queue.emplace_back(
            Event{timeout, timer->event_fifo_id++, userdata, event_type});
        std::push_heap(timer->event_queue.begin(), timer->event_queue.end(), std::greater<>());
    } else {
        timer->ts_queue.Push(Event{static_cast<s64>(timer->GetTicks() + cycles_into_future), 0,
                                   userdata, event_type});
    }
}

void Timing::UnscheduleEvent(const TimingEventType* event_type, u64 userdata) {
    for (auto timer : timers) {
        auto itr = std::remove_if(
            timer->event_queue.begin(), timer->event_queue.end(),
            [&](const Event& e) { return e.type == event_type && e.userdata == userdata; });

        // Removing random items breaks the invariant so we have to re-establish it.
        if (itr != timer->event_queue.end()) {
            timer->event_queue.erase(itr, timer->event_queue.end());
            std::make_heap(timer->event_queue.begin(), timer->event_queue.end(), std::greater<>());
        }
    }
    // TODO:remove events from ts_queue
}

void Timing::RemoveEvent(const TimingEventType* event_type) {
    for (auto timer : timers) {
        auto itr = std::remove_if(timer->event_queue.begin(), timer->event_queue.end(),
                                  [&](const Event& e) { return e.type == event_type; });

        // Removing random items breaks the invariant so we have to re-establish it.
        if (itr != timer->event_queue.end()) {
            timer->event_queue.erase(itr, timer->event_queue.end());
            std::make_heap(timer->event_queue.begin(), timer->event_queue.end(), std::greater<>());
        }
    }
    // TODO:remove events from ts_queue
}

void Timing::SetCurrentTimer(std::size_t core_id) {
    current_timer = timers[core_id];
}

s64 Timing::GetTicks() const {
    return current_timer->GetTicks();
}

s64 Timing::GetGlobalTicks() const {
    return global_timer;
}

std::chrono::microseconds Timing::GetGlobalTimeUs() const {
    return std::chrono::microseconds{GetTicks() * 1000000 / BASE_CLOCK_RATE_ARM11};
}

std::shared_ptr<Timing::Timer> Timing::GetTimer(std::size_t cpu_id) {
    return timers[cpu_id];
}

Timing::Timer::Timer(double cpu_clock_scale_) : cpu_clock_scale(cpu_clock_scale_) {}

Timing::Timer::~Timer() {
    MoveEvents();
}

u64 Timing::Timer::GetTicks() const {
    u64 ticks = static_cast<u64>(executed_ticks);
    if (!is_timer_sane) {
        ticks += slice_length - downcount;
    }
    return ticks;
}

void Timing::Timer::AddTicks(u64 ticks) {
    downcount -= static_cast<u64>(ticks * cpu_clock_scale);
}

u64 Timing::Timer::GetIdleTicks() const {
    return static_cast<u64>(idled_cycles);
}

void Timing::Timer::ForceExceptionCheck(s64 cycles) {
    cycles = std::max<s64>(0, cycles);
    if (downcount > cycles) {
        slice_length -= downcount - cycles;
        downcount = cycles;
    }
}

void Timing::Timer::MoveEvents() {
    for (Event ev; ts_queue.Pop(ev);) {
        ev.fifo_order = event_fifo_id++;
        event_queue.emplace_back(std::move(ev));
        std::push_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }
}

s64 Timing::Timer::GetMaxSliceLength() const {
    auto next_event = std::find_if(event_queue.begin(), event_queue.end(),
                                   [&](const Event& e) { return e.time - executed_ticks > 0; });
    if (next_event != event_queue.end()) {
        return next_event->time - executed_ticks;
    }
    return MAX_SLICE_LENGTH;
}

void Timing::Timer::Advance(s64 max_slice_length) {
    MoveEvents();

    s64 cycles_executed = slice_length - downcount;
    idled_cycles = 0;
    executed_ticks += cycles_executed;
    slice_length = max_slice_length;

    is_timer_sane = true;

    while (!event_queue.empty() && event_queue.front().time <= executed_ticks) {
        Event evt = std::move(event_queue.front());
        std::pop_heap(event_queue.begin(), event_queue.end(), std::greater<>());
        event_queue.pop_back();
        evt.type->callback(evt.userdata, executed_ticks - evt.time);
    }

    is_timer_sane = false;

    // Still events left (scheduled in the future)
    if (!event_queue.empty()) {
        slice_length = static_cast<int>(
            std::min<s64>(event_queue.front().time - executed_ticks, max_slice_length));
    }

    downcount = slice_length;
}

void Timing::Timer::Idle() {
    idled_cycles += downcount;
    downcount = 0;
}

s64 Timing::Timer::GetDowncount() const {
    return downcount;
}

} // namespace Core
