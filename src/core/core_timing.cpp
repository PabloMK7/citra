// Copyright (c) 2012- PPSSPP Project / Dolphin Project.
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <mutex>
#include <vector>

#include "common/chunk_file.h"
#include "common/logging/log.h"
#include "common/string_util.h"

#include "core/arm/arm_interface.h"
#include "core/core.h"
#include "core/core_timing.h"

int g_clock_rate_arm11 = 268123480;

// is this really necessary?
#define INITIAL_SLICE_LENGTH 20000
#define MAX_SLICE_LENGTH 100000000

namespace CoreTiming
{
struct EventType
{
    EventType() {}

    EventType(TimedCallback cb, const char* n)
        : callback(cb), name(n) {}

    TimedCallback callback;
    const char* name;
};

static std::vector<EventType> event_types;

struct BaseEvent
{
    s64 time;
    u64 userdata;
    int type;
};

typedef LinkedListItem<BaseEvent> Event;

static Event* first;
static Event* ts_first;
static Event* ts_last;

// event pools
static Event* event_pool = nullptr;
static Event* event_ts_pool = nullptr;
static int allocated_ts_events = 0;
// Optimization to skip MoveEvents when possible.
static std::atomic<bool> has_ts_events(false);

int g_slice_length;

static s64 global_timer;
static s64 idled_cycles;
static s64 last_global_time_ticks;
static s64 last_global_time_us;

static std::recursive_mutex external_event_section;

// Warning: not included in save state.
using AdvanceCallback = void(int cycles_executed);
static AdvanceCallback* advance_callback = nullptr;
static std::vector<MHzChangeCallback> mhz_change_callbacks;

static void FireMhzChange() {
    for (auto callback : mhz_change_callbacks)
        callback();
}

void SetClockFrequencyMHz(int cpu_mhz) {
    // When the mhz changes, we keep track of what "time" it was before hand.
    // This way, time always moves forward, even if mhz is changed.
    last_global_time_us = GetGlobalTimeUs();
    last_global_time_ticks = GetTicks();

    g_clock_rate_arm11 = cpu_mhz * 1000000;
    // TODO: Rescale times of scheduled events?

    FireMhzChange();
}

int GetClockFrequencyMHz() {
    return g_clock_rate_arm11 / 1000000;
}

u64 GetGlobalTimeUs() {
    s64 ticks_since_last = GetTicks() - last_global_time_ticks;
    int freq = GetClockFrequencyMHz();
    s64 us_since_last = ticks_since_last / freq;
    return last_global_time_us + us_since_last;
}

static Event* GetNewEvent() {
    if (!event_pool)
        return new Event;

    Event* event = event_pool;
    event_pool = event->next;
    return event;
}

static Event* GetNewTsEvent() {
    allocated_ts_events++;

    if (!event_ts_pool)
        return new Event;

    Event* event = event_ts_pool;
    event_ts_pool = event->next;
    return event;
}

static void FreeEvent(Event* event) {
    event->next = event_pool;
    event_pool = event;
}

static void FreeTsEvent(Event* event) {
    event->next = event_ts_pool;
    event_ts_pool = event;
    allocated_ts_events--;
}

int RegisterEvent(const char* name, TimedCallback callback) {
    event_types.emplace_back(callback, name);
    return (int)event_types.size() - 1;
}

static void AntiCrashCallback(u64 userdata, int cycles_late) {
    LOG_CRITICAL(Core_Timing, "Savestate broken: an unregistered event was called.");
    Core::Halt("invalid timing events");
}

void RestoreRegisterEvent(int event_type, const char* name, TimedCallback callback) {
    if (event_type >= (int)event_types.size())
        event_types.resize(event_type + 1, EventType(AntiCrashCallback, "INVALID EVENT"));

    event_types[event_type] = EventType(callback, name);
}

void UnregisterAllEvents() {
    if (first)
        LOG_ERROR(Core_Timing, "Cannot unregister events with events pending");
    event_types.clear();
}

void Init() {
    Core::g_app_core->down_count = INITIAL_SLICE_LENGTH;
    g_slice_length = INITIAL_SLICE_LENGTH;
    global_timer = 0;
    idled_cycles = 0;
    last_global_time_ticks = 0;
    last_global_time_us = 0;
    has_ts_events = 0;
    mhz_change_callbacks.clear();

    first = nullptr;
    ts_first = nullptr;
    ts_last = nullptr;

    event_pool = nullptr;
    event_ts_pool = nullptr;
    allocated_ts_events = 0;

    advance_callback = nullptr;
}

void Shutdown() {
    MoveEvents();
    ClearPendingEvents();
    UnregisterAllEvents();

    while (event_pool) {
        Event* event = event_pool;
        event_pool = event->next;
        delete event;
    }

    std::lock_guard<std::recursive_mutex> lock(external_event_section);
    while (event_ts_pool) {
        Event* event = event_ts_pool;
        event_ts_pool = event->next;
        delete event;
    }
}

u64 GetTicks() {
    return (u64)global_timer + g_slice_length - Core::g_app_core->down_count;
}

u64 GetIdleTicks() {
    return (u64)idled_cycles;
}


// This is to be called when outside threads, such as the graphics thread, wants to
// schedule things to be executed on the main thread.
void ScheduleEvent_Threadsafe(s64 cycles_into_future, int event_type, u64 userdata) {
    std::lock_guard<std::recursive_mutex> lock(external_event_section);
    Event* new_event = GetNewTsEvent();
    new_event->time = GetTicks() + cycles_into_future;
    new_event->type = event_type;
    new_event->next = 0;
    new_event->userdata = userdata;
    if (!ts_first)
        ts_first = new_event;
    if (ts_last)
        ts_last->next = new_event;
    ts_last = new_event;

    has_ts_events = true;
}

// Same as ScheduleEvent_Threadsafe(0, ...) EXCEPT if we are already on the CPU thread
// in which case the event will get handled immediately, before returning.
void ScheduleEvent_Threadsafe_Immediate(int event_type, u64 userdata) {
    if (false) //Core::IsCPUThread())
    {
        std::lock_guard<std::recursive_mutex> lock(external_event_section);
        event_types[event_type].callback(userdata, 0);
    }
    else
        ScheduleEvent_Threadsafe(0, event_type, userdata);
}

void ClearPendingEvents() {
    while (first) {
        Event* event = first->next;
        FreeEvent(first);
        first = event;
    }
}

static void AddEventToQueue(Event* new_event) {
    Event* prev_event = nullptr;
    Event** next_event = &first;
    for (;;) {
        Event*& next = *next_event;
        if (!next || new_event->time < next->time) {
            new_event->next = next;
            next = new_event;
            break;
        }
        prev_event = next;
        next_event = &prev_event->next;
    }
}

void ScheduleEvent(s64 cycles_into_future, int event_type, u64 userdata) {
    Event* new_event = GetNewEvent();
    new_event->userdata = userdata;
    new_event->type = event_type;
    new_event->time = GetTicks() + cycles_into_future;
    AddEventToQueue(new_event);
}

s64 UnscheduleEvent(int event_type, u64 userdata) {
    s64 result = 0;
    if (!first)
        return result;
    while (first) {
        if (first->type == event_type && first->userdata == userdata) {
            result = first->time - GetTicks();

            Event* next = first->next;
            FreeEvent(first);
            first = next;
        } else {
            break;
        }
    }
    if (!first)
        return result;

    Event* prev_event = first;
    Event* ptr = prev_event->next;

    while (ptr) {
        if (ptr->type == event_type && ptr->userdata == userdata) {
            result = ptr->time - GetTicks();

            prev_event->next = ptr->next;
            FreeEvent(ptr);
            ptr = prev_event->next;
        } else {
            prev_event = ptr;
            ptr = ptr->next;
        }
    }

    return result;
}

s64 UnscheduleThreadsafeEvent(int event_type, u64 userdata) {
    s64 result = 0;
    std::lock_guard<std::recursive_mutex> lock(external_event_section);
    if (!ts_first)
        return result;

    while (ts_first) {
        if (ts_first->type == event_type && ts_first->userdata == userdata) {
            result = ts_first->time - GetTicks();

            Event* next = ts_first->next;
            FreeTsEvent(ts_first);
            ts_first = next;
        } else {
            break;
        }
    }

    if (!ts_first)
    {
        ts_last = nullptr;
        return result;
    }

    Event* prev_event = ts_first;
    Event* next = prev_event->next;
    while (next) {
        if (next->type == event_type && next->userdata == userdata) {
            result = next->time - GetTicks();

            prev_event->next = next->next;
            if (next == ts_last)
                ts_last = prev_event;
            FreeTsEvent(next);
            next = prev_event->next;
        } else {
            prev_event = next;
            next = next->next;
        }
    }

    return result;
}

// Warning: not included in save state.
void RegisterAdvanceCallback(AdvanceCallback* callback) {
    advance_callback = callback;
}

void RegisterMHzChangeCallback(MHzChangeCallback callback) {
    mhz_change_callbacks.push_back(callback);
}

bool IsScheduled(int event_type) {
    if (!first)
        return false;
    Event* event = first;
    while (event) {
        if (event->type == event_type)
            return true;
        event = event->next;
    }
    return false;
}

void RemoveEvent(int event_type) {
    if (!first)
        return;
    while (first) {
        if (first->type == event_type) {
            Event *next = first->next;
            FreeEvent(first);
            first = next;
        } else {
            break;
        }
    }
    if (!first)
        return;
    Event* prev = first;
    Event* next = prev->next;
    while (next) {
        if (next->type == event_type) {
            prev->next = next->next;
            FreeEvent(next);
            next = prev->next;
        } else {
            prev = next;
            next = next->next;
        }
    }
}

void RemoveThreadsafeEvent(int event_type) {
    std::lock_guard<std::recursive_mutex> lock(external_event_section);
    if (!ts_first)
        return;

    while (ts_first) {
        if (ts_first->type == event_type) {
            Event* next = ts_first->next;
            FreeTsEvent(ts_first);
            ts_first = next;
        } else {
            break;
        }
    }

    if (!ts_first) {
        ts_last = nullptr;
        return;
    }

    Event* prev = ts_first;
    Event* next = prev->next;
    while (next) {
        if (next->type == event_type) {
            prev->next = next->next;
            if (next == ts_last)
                ts_last = prev;
            FreeTsEvent(next);
            next = prev->next;
        } else {
            prev = next;
            next = next->next;
        }
    }
}

void RemoveAllEvents(int event_type) {
    RemoveThreadsafeEvent(event_type);
    RemoveEvent(event_type);
}

// This raise only the events required while the fifo is processing data
void ProcessFifoWaitEvents() {
    while (first) {
        if (first->time <= (s64)GetTicks()) {
            Event* evt = first;
            first = first->next;
            event_types[evt->type].callback(evt->userdata, (int)(GetTicks() - evt->time));
            FreeEvent(evt);
        } else {
            break;
        }
    }
}

void MoveEvents() {
    has_ts_events = false;

    std::lock_guard<std::recursive_mutex> lock(external_event_section);
    // Move events from async queue into main queue
    while (ts_first) {
        Event* next = ts_first->next;
        AddEventToQueue(ts_first);
        ts_first = next;
    }
    ts_last = nullptr;

    // Move free events to threadsafe pool
    while (allocated_ts_events > 0 && event_pool) {
        Event* event = event_pool;
        event_pool = event->next;
        event->next = event_ts_pool;
        event_ts_pool = event;
        allocated_ts_events--;
    }
}

void ForceCheck() {
    s64 cycles_executed = g_slice_length - Core::g_app_core->down_count;
    global_timer += cycles_executed;
    // This will cause us to check for new events immediately.
    Core::g_app_core->down_count = 0;
    // But let's not eat a bunch more time in Advance() because of this.
    g_slice_length = 0;
}

void Advance() {
    s64 cycles_executed = g_slice_length - Core::g_app_core->down_count;
    global_timer += cycles_executed;
    Core::g_app_core->down_count = g_slice_length;

    if (has_ts_events)
        MoveEvents();
    ProcessFifoWaitEvents();

    if (!first) {
        if (g_slice_length < 10000) {
            g_slice_length += 10000;
            Core::g_app_core->down_count += g_slice_length;
        }
    } else {
        // Note that events can eat cycles as well.
        int target = (int)(first->time - global_timer);
        if (target > MAX_SLICE_LENGTH)
            target = MAX_SLICE_LENGTH;

        const int diff = target - g_slice_length;
        g_slice_length += diff;
        Core::g_app_core->down_count += diff;
    }
    if (advance_callback)
        advance_callback(static_cast<int>(cycles_executed));
}

void LogPendingEvents() {
    Event* event = first;
    while (event) {
        //LOG_TRACE(Core_Timing, "PENDING: Now: %lld Pending: %lld Type: %d", globalTimer, next->time, next->type);
        event = event->next;
    }
}

void Idle(int max_idle) {
    s64 cycles_down = Core::g_app_core->down_count;
    if (max_idle != 0 && cycles_down > max_idle)
        cycles_down = max_idle;

    if (first && cycles_down > 0) {
        s64 cycles_executed = g_slice_length - Core::g_app_core->down_count;
        s64 cycles_next_event = first->time - global_timer;

        if (cycles_next_event < cycles_executed + cycles_down) {
            cycles_down = cycles_next_event - cycles_executed;
            // Now, now... no time machines, please.
            if (cycles_down < 0)
                cycles_down = 0;
        }
    }

    LOG_TRACE(Core_Timing, "Idle for %i cycles! (%f ms)", cycles_down, cycles_down / (float)(g_clock_rate_arm11 * 0.001f));

    idled_cycles += cycles_down;
    Core::g_app_core->down_count -= cycles_down;
    if (Core::g_app_core->down_count == 0)
        Core::g_app_core->down_count = -1;
}

std::string GetScheduledEventsSummary() {
    Event* event = first;
    std::string text = "Scheduled events\n";
    text.reserve(1000);
    while (event) {
        unsigned int t = event->type;
        if (t >= event_types.size())
            LOG_ERROR(Core_Timing, "Invalid event type"); // %i", t);
        const char* name = event_types[event->type].name;
        if (!name)
            name = "[unknown]";
        text += Common::StringFromFormat("%s : %i %08x%08x\n", name, (int)event->time,
                (u32)(event->userdata >> 32), (u32)(event->userdata));
        event = event->next;
    }
    return text;
}

} // namespace
