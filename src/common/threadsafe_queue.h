// Copyright 2010 Dolphin Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

// a simple lockless thread-safe,
// single reader, single writer queue

#include <algorithm>
#include <atomic>
#include <condition_variable>
#include <cstddef>
#include <mutex>
#include "common/common_types.h"

namespace Common {
template <typename T, bool NeedSize = true>
class SPSCQueue {
public:
    SPSCQueue() : size(0) {
        write_ptr = read_ptr = new ElementPtr();
    }
    ~SPSCQueue() {
        // this will empty out the whole queue
        delete read_ptr;
    }

    u32 Size() const {
        static_assert(NeedSize, "using Size() on FifoQueue without NeedSize");
        return size.load();
    }

    bool Empty() const {
        return !read_ptr->next.load();
    }

    T& Front() const {
        return read_ptr->current;
    }

    template <typename Arg>
    void Push(Arg&& t) {
        // create the element, add it to the queue
        write_ptr->current = std::forward<Arg>(t);
        // set the next pointer to a new element ptr
        // then advance the write pointer
        ElementPtr* new_ptr = new ElementPtr();
        write_ptr->next.store(new_ptr, std::memory_order_release);
        write_ptr = new_ptr;
        if (NeedSize)
            size++;
        cv.notify_one();
    }

    void Pop() {
        if (NeedSize)
            size--;
        ElementPtr* tmpptr = read_ptr;
        // advance the read pointer
        read_ptr = tmpptr->next.load();
        // set the next element to nullptr to stop the recursive deletion
        tmpptr->next.store(nullptr);
        delete tmpptr; // this also deletes the element
    }

    bool Pop(T& t) {
        if (Empty())
            return false;

        if (NeedSize)
            size--;

        ElementPtr* tmpptr = read_ptr;
        read_ptr = tmpptr->next.load(std::memory_order_acquire);
        t = std::move(tmpptr->current);
        tmpptr->next.store(nullptr);
        delete tmpptr;
        return true;
    }

    T PopWait() {
        if (Empty()) {
            std::unique_lock<std::mutex> lock(cv_mutex);
            cv.wait(lock, [this]() { return !Empty(); });
        }
        T t;
        Pop(t);
        return t;
    }

    // not thread-safe
    void Clear() {
        size.store(0);
        delete read_ptr;
        write_ptr = read_ptr = new ElementPtr();
    }

private:
    // stores a pointer to element
    // and a pointer to the next ElementPtr
    class ElementPtr {
    public:
        ElementPtr() : next(nullptr) {}
        ~ElementPtr() {
            ElementPtr* next_ptr = next.load();

            if (next_ptr)
                delete next_ptr;
        }

        T current;
        std::atomic<ElementPtr*> next;
    };

    ElementPtr* write_ptr;
    ElementPtr* read_ptr;
    std::atomic<u32> size;
    std::mutex cv_mutex;
    std::condition_variable cv;
};

// a simple thread-safe,
// single reader, multiple writer queue

template <typename T, bool NeedSize = true>
class MPSCQueue {
public:
    u32 Size() const {
        return spsc_queue.Size();
    }

    bool Empty() const {
        return spsc_queue.Empty();
    }

    T& Front() const {
        return spsc_queue.Front();
    }

    template <typename Arg>
    void Push(Arg&& t) {
        std::lock_guard<std::mutex> lock(write_lock);
        spsc_queue.Push(t);
    }

    void Pop() {
        return spsc_queue.Pop();
    }

    bool Pop(T& t) {
        return spsc_queue.Pop(t);
    }

    T PopWait() {
        return spsc_queue.PopWait();
    }

    // not thread-safe
    void Clear() {
        spsc_queue.Clear();
    }

private:
    SPSCQueue<T, NeedSize> spsc_queue;
    std::mutex write_lock;
};
} // namespace Common
