// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <chrono>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include <thread>
#include "common/common_types.h"

namespace Common {

int CurrentThreadId();

void SetThreadAffinity(std::thread::native_handle_type thread, u32 mask);
void SetCurrentThreadAffinity(u32 mask);

class Event {
public:
    Event() : is_set(false) {}

    void Set() {
        std::lock_guard<std::mutex> lk(mutex);
        if (!is_set) {
            is_set = true;
            condvar.notify_one();
        }
    }

    void Wait() {
        std::unique_lock<std::mutex> lk(mutex);
        condvar.wait(lk, [&] { return is_set; });
        is_set = false;
    }

    template <class Duration>
    bool WaitFor(const std::chrono::duration<Duration>& time) {
        std::unique_lock<std::mutex> lk(mutex);
        if (!condvar.wait_for(lk, time, [this] { return is_set; }))
            return false;
        is_set = false;
        return true;
    }

    template <class Clock, class Duration>
    bool WaitUntil(const std::chrono::time_point<Clock, Duration>& time) {
        std::unique_lock<std::mutex> lk(mutex);
        if (!condvar.wait_until(lk, time, [this] { return is_set; }))
            return false;
        is_set = false;
        return true;
    }

    void Reset() {
        std::unique_lock<std::mutex> lk(mutex);
        // no other action required, since wait loops on the predicate and any lingering signal will
        // get cleared on the first iteration
        is_set = false;
    }

private:
    bool is_set;
    std::condition_variable condvar;
    std::mutex mutex;
};

class Barrier {
public:
    explicit Barrier(std::size_t count_) : count(count_), waiting(0), generation(0) {}

    /// Blocks until all "count" threads have called Sync()
    void Sync() {
        std::unique_lock<std::mutex> lk(mutex);
        const std::size_t current_generation = generation;

        if (++waiting == count) {
            generation++;
            waiting = 0;
            condvar.notify_all();
        } else {
            condvar.wait(lk,
                         [this, current_generation] { return current_generation != generation; });
        }
    }

private:
    std::condition_variable condvar;
    std::mutex mutex;
    const std::size_t count;
    std::size_t waiting;
    std::size_t generation; // Incremented once each time the barrier is used
};

void SleepCurrentThread(int ms);
void SwitchCurrentThread(); // On Linux, this is equal to sleep 1ms
void SetCurrentThreadName(const char* name);

} // namespace Common
