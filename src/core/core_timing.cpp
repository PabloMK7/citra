// Copyright 2008 Dolphin Emulator Project / 2017 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include "core/core_timing.h"

#include <algorithm>
#include <cinttypes>
#include <mutex>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>
#include "common/assert.h"
#include "common/logging/log.h"
#include "common/thread.h"
#include "common/threadsafe_queue.h"

namespace CoreTiming {

static s64 global_timer;
static s64 slice_length;
static s64 downcount;

struct EventType {
    TimedCallback callback;
    const std::string* name;
};

struct Event {
    s64 time;
    u64 fifo_order;
    u64 userdata;
    const EventType* type;
};

// Sort by time, unless the times are the same, in which case sort by the order added to the queue
static bool operator>(const Event& left, const Event& right) {
    return std::tie(left.time, left.fifo_order) > std::tie(right.time, right.fifo_order);
}

static bool operator<(const Event& left, const Event& right) {
    return std::tie(left.time, left.fifo_order) < std::tie(right.time, right.fifo_order);
}

// unordered_map stores each element separately as a linked list node so pointers to elements
// remain stable regardless of rehashes/resizing.
static std::unordered_map<std::string, EventType> event_types;

// The queue is a min-heap using std::make_heap/push_heap/pop_heap.
// We don't use std::priority_queue because we need to be able to serialize, unserialize and
// erase arbitrary events (RemoveEvent()) regardless of the queue order. These aren't accomodated
// by the standard adaptor class.
static std::vector<Event> event_queue;
static u64 event_fifo_id;
// the queue for storing the events from other threads threadsafe until they will be added
// to the event_queue by the emu thread
static Common::MPSCQueue<Event, false> ts_queue;

static constexpr int MAX_SLICE_LENGTH = 20000;

static s64 idled_cycles;

// Are we in a function that has been called from Advance()
// If events are sheduled from a function that gets called from Advance(),
// don't change slice_length and downcount.
static bool is_global_timer_sane;

static EventType* ev_lost = nullptr;

static void EmptyTimedCallback(u64 userdata, s64 cyclesLate) {}

EventType* RegisterEvent(const std::string& name, TimedCallback callback) {
    // check for existing type with same name.
    // we want event type names to remain unique so that we can use them for serialization.
    ASSERT_MSG(event_types.find(name) == event_types.end(),
               "CoreTiming Event \"{}\" is already registered. Events should only be registered "
               "during Init to avoid breaking save states.",
               name);

    auto info = event_types.emplace(name, EventType{callback, nullptr});
    EventType* event_type = &info.first->second;
    event_type->name = &info.first->first;
    return event_type;
}

void UnregisterAllEvents() {
    ASSERT_MSG(event_queue.empty(), "Cannot unregister events with events pending");
    event_types.clear();
}

void Init() {
    downcount = MAX_SLICE_LENGTH;
    slice_length = MAX_SLICE_LENGTH;
    global_timer = 0;
    idled_cycles = 0;

    // The time between CoreTiming being intialized and the first call to Advance() is considered
    // the slice boundary between slice -1 and slice 0. Dispatcher loops must call Advance() before
    // executing the first cycle of each slice to prepare the slice length and downcount for
    // that slice.
    is_global_timer_sane = true;

    event_fifo_id = 0;
    ev_lost = RegisterEvent("_lost_event", &EmptyTimedCallback);
}

void Shutdown() {
    MoveEvents();
    ClearPendingEvents();
    UnregisterAllEvents();
}

// This should only be called from the CPU thread. If you are calling
// it from any other thread, you are doing something evil
u64 GetTicks() {
    u64 ticks = static_cast<u64>(global_timer);
    if (!is_global_timer_sane) {
        ticks += slice_length - downcount;
    }
    return ticks;
}

void AddTicks(u64 ticks) {
    downcount -= ticks;
}

u64 GetIdleTicks() {
    return static_cast<u64>(idled_cycles);
}

void ClearPendingEvents() {
    event_queue.clear();
}

void ScheduleEvent(s64 cycles_into_future, const EventType* event_type, u64 userdata) {
    ASSERT(event_type != nullptr);
    s64 timeout = GetTicks() + cycles_into_future;

    // If this event needs to be scheduled before the next advance(), force one early
    if (!is_global_timer_sane)
        ForceExceptionCheck(cycles_into_future);

    event_queue.emplace_back(Event{timeout, event_fifo_id++, userdata, event_type});
    std::push_heap(event_queue.begin(), event_queue.end(), std::greater<>());
}

void ScheduleEventThreadsafe(s64 cycles_into_future, const EventType* event_type, u64 userdata) {
    ts_queue.Push(Event{global_timer + cycles_into_future, 0, userdata, event_type});
}

void UnscheduleEvent(const EventType* event_type, u64 userdata) {
    auto itr = std::remove_if(event_queue.begin(), event_queue.end(), [&](const Event& e) {
        return e.type == event_type && e.userdata == userdata;
    });

    // Removing random items breaks the invariant so we have to re-establish it.
    if (itr != event_queue.end()) {
        event_queue.erase(itr, event_queue.end());
        std::make_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }
}

void RemoveEvent(const EventType* event_type) {
    auto itr = std::remove_if(event_queue.begin(), event_queue.end(),
                              [&](const Event& e) { return e.type == event_type; });

    // Removing random items breaks the invariant so we have to re-establish it.
    if (itr != event_queue.end()) {
        event_queue.erase(itr, event_queue.end());
        std::make_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }
}

void RemoveNormalAndThreadsafeEvent(const EventType* event_type) {
    MoveEvents();
    RemoveEvent(event_type);
}

void ForceExceptionCheck(s64 cycles) {
    cycles = std::max<s64>(0, cycles);
    if (downcount > cycles) {
        slice_length -= downcount - cycles;
        downcount = cycles;
    }
}

void MoveEvents() {
    for (Event ev; ts_queue.Pop(ev);) {
        ev.fifo_order = event_fifo_id++;
        event_queue.emplace_back(std::move(ev));
        std::push_heap(event_queue.begin(), event_queue.end(), std::greater<>());
    }
}

void Advance() {
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

void Idle() {
    idled_cycles += downcount;
    downcount = 0;
}

std::chrono::microseconds GetGlobalTimeUs() {
    return std::chrono::microseconds{GetTicks() * 1000000 / BASE_CLOCK_RATE_ARM11};
}

s64 GetDowncount() {
    return downcount;
}

} // namespace CoreTiming
