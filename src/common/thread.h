// Copyright 2013 Dolphin Emulator Project / 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <thread>
#include <condition_variable>
#include <mutex>

#include "common/common_types.h"

// Support for C++11's thread_local keyword was surprisingly spotty in compilers until very
// recently. Fortunately, thread local variables have been well supported for compilers for a while,
// but with semantics supporting only POD types, so we can use a few defines to get some amount of
// backwards compat support.
// WARNING: This only works correctly with POD types.
#if defined(__clang__)
#   if !__has_feature(cxx_thread_local)
#       define thread_local __thread
#   endif
#elif defined(__GNUC__)
#   if __GNUC__ < 4 || (__GNUC__ == 4 && __GNUC_MINOR__ < 8)
#       define thread_local __thread
#   endif
#elif defined(_MSC_VER)
#   if _MSC_VER < 1900
#       define thread_local __declspec(thread)
#   endif
#endif

namespace Common
{

int CurrentThreadId();

void SetThreadAffinity(std::thread::native_handle_type thread, u32 mask);
void SetCurrentThreadAffinity(u32 mask);

class Event {
public:
    Event() : is_set(false) {}

    void Set() {
        std::lock_guard<std::mutex> lk(m_mutex);
        if (!is_set) {
            is_set = true;
            m_condvar.notify_one();
        }
    }

    void Wait() {
        std::unique_lock<std::mutex> lk(m_mutex);
        m_condvar.wait(lk, [&]{ return is_set; });
        is_set = false;
    }

    void Reset() {
        std::unique_lock<std::mutex> lk(m_mutex);
        // no other action required, since wait loops on the predicate and any lingering signal will get cleared on the first iteration
        is_set = false;
    }

private:
    bool is_set;
    std::condition_variable m_condvar;
    std::mutex m_mutex;
};

class Barrier {
public:
    Barrier(size_t count) : m_count(count), m_waiting(0) {}

    /// Blocks until all "count" threads have called Sync()
    void Sync() {
        std::unique_lock<std::mutex> lk(m_mutex);

        // TODO: broken when next round of Sync()s
        // is entered before all waiting threads return from the notify_all

        if (++m_waiting == m_count) {
            m_waiting = 0;
            m_condvar.notify_all();
        } else {
            m_condvar.wait(lk, [&]{ return m_waiting == 0; });
        }
    }

private:
    std::condition_variable m_condvar;
    std::mutex m_mutex;
    const size_t m_count;
    size_t m_waiting;
};

void SleepCurrentThread(int ms);
void SwitchCurrentThread();    // On Linux, this is equal to sleep 1ms

// Use this function during a spin-wait to make the current thread
// relax while another thread is working. This may be more efficient
// than using events because event functions use kernel calls.
inline void YieldCPU()
{
    std::this_thread::yield();
}

void SetCurrentThreadName(const char *name);

} // namespace Common
