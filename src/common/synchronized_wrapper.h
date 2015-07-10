// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <algorithm>
#include <mutex>

namespace Common {

/**
 * Wraps an object, only allowing access to it via a locking reference wrapper. Good to ensure no
 * one forgets to lock a mutex before acessing an object. To access the wrapped object construct a
 * SyncronizedRef on this wrapper. Inspired by Rust's Mutex type (http://doc.rust-lang.org/std/sync/struct.Mutex.html).
 */
template <typename T>
class SynchronizedWrapper {
public:
    template <typename... Args>
    SynchronizedWrapper(Args&&... args) :
        data(std::forward<Args>(args)...) {
    }

private:
    template <typename U>
    friend class SynchronizedRef;

    std::mutex mutex;
    T data;
};

/**
 * Synchronized reference, that keeps a SynchronizedWrapper's mutex locked during its lifetime. This
 * greatly reduces the chance that someone will access the wrapped resource without locking the
 * mutex.
 */
template <typename T>
class SynchronizedRef {
public:
    SynchronizedRef(SynchronizedWrapper<T>& wrapper) : wrapper(&wrapper) {
        wrapper.mutex.lock();
    }

    SynchronizedRef(SynchronizedRef&) = delete;
    SynchronizedRef(SynchronizedRef&& o) : wrapper(o.wrapper) {
        o.wrapper = nullptr;
    }

    ~SynchronizedRef() {
        if (wrapper)
            wrapper->mutex.unlock();
    }

    SynchronizedRef& operator=(SynchronizedRef&) = delete;
    SynchronizedRef& operator=(SynchronizedRef&& o) {
        std::swap(wrapper, o.wrapper);
    }

    T& operator*() { return wrapper->data; }
    const T& operator*() const { return wrapper->data; }

    T* operator->() { return &wrapper->data; }
    const T* operator->() const { return &wrapper->data; }

private:
    SynchronizedWrapper<T>* wrapper;
};

} // namespace Common
