// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <deque>
#include <boost/range/algorithm_ext/erase.hpp>

namespace Common {

template <class T, unsigned int N>
struct ThreadQueueList {
    // TODO(yuriks): If performance proves to be a problem, the std::deques can be replaced with
    //               (dynamically resizable) circular buffers to remove their overhead when
    //               inserting and popping.

    typedef unsigned int Priority;

    // Number of priority levels. (Valid levels are [0..NUM_QUEUES).)
    static const Priority NUM_QUEUES = N;

    ThreadQueueList() {
        first = nullptr;
    }

    // Only for debugging, returns priority level.
    Priority contains(const T& uid) {
        for (Priority i = 0; i < NUM_QUEUES; ++i) {
            Queue& cur = queues[i];
            if (std::find(cur.data.cbegin(), cur.data.cend(), uid) != cur.data.cend()) {
                return i;
            }
        }

        return -1;
    }

    T get_first() {
        Queue* cur = first;
        while (cur != nullptr) {
            if (!cur->data.empty()) {
                return cur->data.front();
            }
            cur = cur->next_nonempty;
        }

        return T();
    }

    T pop_first() {
        Queue* cur = first;
        while (cur != nullptr) {
            if (!cur->data.empty()) {
                auto tmp = std::move(cur->data.front());
                cur->data.pop_front();
                return tmp;
            }
            cur = cur->next_nonempty;
        }

        return T();
    }

    T pop_first_better(Priority priority) {
        Queue* cur = first;
        Queue* stop = &queues[priority];
        while (cur < stop) {
            if (!cur->data.empty()) {
                auto tmp = std::move(cur->data.front());
                cur->data.pop_front();
                return tmp;
            }
            cur = cur->next_nonempty;
        }

        return T();
    }

    void push_front(Priority priority, const T& thread_id) {
        Queue* cur = &queues[priority];
        cur->data.push_front(thread_id);
    }

    void push_back(Priority priority, const T& thread_id) {
        Queue* cur = &queues[priority];
        cur->data.push_back(thread_id);
    }

    void move(const T& thread_id, Priority old_priority, Priority new_priority) {
        remove(old_priority, thread_id);
        prepare(new_priority);
        push_back(new_priority, thread_id);
    }

    void remove(Priority priority, const T& thread_id) {
        Queue* cur = &queues[priority];
        boost::remove_erase(cur->data, thread_id);
    }

    void rotate(Priority priority) {
        Queue* cur = &queues[priority];

        if (cur->data.size() > 1) {
            cur->data.push_back(std::move(cur->data.front()));
            cur->data.pop_front();
        }
    }

    void clear() {
        queues.fill(Queue());
        first = nullptr;
    }

    bool empty(Priority priority) const {
        const Queue* cur = &queues[priority];
        return cur->data.empty();
    }

    void prepare(Priority priority) {
        Queue* cur = &queues[priority];
        if (cur->next_nonempty == UnlinkedTag())
            link(priority);
    }

private:
    struct Queue {
        // Points to the next active priority, skipping over ones that have never been used.
        Queue* next_nonempty = UnlinkedTag();
        // Double-ended queue of threads in this priority level
        std::deque<T> data;
    };

    /// Special tag used to mark priority levels that have never been used.
    static Queue* UnlinkedTag() {
        return reinterpret_cast<Queue*>(1);
    }

    void link(Priority priority) {
        Queue* cur = &queues[priority];

        for (int i = priority - 1; i >= 0; --i) {
            if (queues[i].next_nonempty != UnlinkedTag()) {
                cur->next_nonempty = queues[i].next_nonempty;
                queues[i].next_nonempty = cur;
                return;
            }
        }

        cur->next_nonempty = first;
        first = cur;
    }

    // The first queue that's ever been used.
    Queue* first;
    // The priority level queues of thread ids.
    std::array<Queue, NUM_QUEUES> queues;
};

} // namespace
