// Copyright 2014 Citra Emulator Project / PPSSPP Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "common/common.h"

namespace Common {

template<class IdType>
struct ThreadQueueList {
    // Number of queues (number of priority levels starting at 0.)
    static const int NUM_QUEUES = 128;

    // Initial number of threads a single queue can handle.
    static const int INITIAL_CAPACITY = 32;

    struct Queue {
        // Next ever-been-used queue (worse priority.)
        Queue *next;
        // First valid item in data.
        int first;
        // One after last valid item in data.
        int end;
        // A too-large array with room on the front and end.
        IdType *data;
        // Size of data array.
        int capacity;
    };

    ThreadQueueList() {
        memset(queues, 0, sizeof(queues));
        first = invalid();
    }

    ~ThreadQueueList() {
        for (int i = 0; i < NUM_QUEUES; ++i)
        {
            if (queues[i].data != NULL)
                free(queues[i].data);
        }
    }

    // Only for debugging, returns priority level.
    int contains(const IdType uid) {
        for (int i = 0; i < NUM_QUEUES; ++i)
        {
            if (queues[i].data == NULL)
                continue;

            Queue *cur = &queues[i];
            for (int j = cur->first; j < cur->end; ++j)
            {
                if (cur->data[j] == uid)
                    return i;
            }
        }

        return -1;
    }

    inline IdType pop_first() {
        Queue *cur = first;
        while (cur != invalid())
        {
            if (cur->end - cur->first > 0)
                return cur->data[cur->first++];
            cur = cur->next;
        }

        //_dbg_assert_msg_(SCEKERNEL, false, "ThreadQueueList should not be empty.");
        return 0;
    }

    inline IdType pop_first_better(u32 priority) {
        Queue *cur = first;
        Queue *stop = &queues[priority];
        while (cur < stop)
        {
            if (cur->end - cur->first > 0)
                return cur->data[cur->first++];
            cur = cur->next;
        }

        return 0;
    }

    inline void push_front(u32 priority, const IdType threadID) {
        Queue *cur = &queues[priority];
        cur->data[--cur->first] = threadID;
        if (cur->first == 0)
            rebalance(priority);
    }

    inline void push_back(u32 priority, const IdType threadID) {
        Queue *cur = &queues[priority];
        cur->data[cur->end++] = threadID;
        if (cur->end == cur->capacity)
            rebalance(priority);
    }

    inline void remove(u32 priority, const IdType threadID) {
        Queue *cur = &queues[priority];
        //_dbg_assert_msg_(SCEKERNEL, cur->next != NULL, "ThreadQueueList::Queue should already be linked up.");

        for (int i = cur->first; i < cur->end; ++i)
        {
            if (cur->data[i] == threadID)
            {
                int remaining = --cur->end - i;
                if (remaining > 0)
                    memmove(&cur->data[i], &cur->data[i + 1], remaining * sizeof(IdType));
                return;
            }
        }

        // Wasn't there.
    }

    inline void rotate(u32 priority) {
        Queue *cur = &queues[priority];
        //_dbg_assert_msg_(SCEKERNEL, cur->next != NULL, "ThreadQueueList::Queue should already be linked up.");

        if (cur->end - cur->first > 1)
        {
            cur->data[cur->end++] = cur->data[cur->first++];
            if (cur->end == cur->capacity)
                rebalance(priority);
        }
    }

    inline void clear() {
        for (int i = 0; i < NUM_QUEUES; ++i)
        {
            if (queues[i].data != NULL)
                free(queues[i].data);
        }
        memset(queues, 0, sizeof(queues));
        first = invalid();
    }

    inline bool empty(u32 priority) const {
        const Queue *cur = &queues[priority];
        return cur->first == cur->end;
    }

    inline void prepare(u32 priority) {
        Queue *cur = &queues[priority];
        if (cur->next == NULL)
            link(priority, INITIAL_CAPACITY);
    }

private:
    Queue *invalid() const {
        return (Queue *) -1;
    }

    void link(u32 priority, int size) {
        //_dbg_assert_msg_(SCEKERNEL, queues[priority].data == NULL, "ThreadQueueList::Queue should only be initialized once.");

        if (size <= INITIAL_CAPACITY)
            size = INITIAL_CAPACITY;
        else
        {
            int goal = size;
            size = INITIAL_CAPACITY;
            while (size < goal)
                size *= 2;
        }
        Queue *cur = &queues[priority];
        cur->data = (IdType *) malloc(sizeof(IdType) * size);
        cur->capacity = size;
        cur->first = size / 2;
        cur->end = size / 2;

        for (int i = (int) priority - 1; i >= 0; --i)
        {
            if (queues[i].next != NULL)
            {
                cur->next = queues[i].next;
                queues[i].next = cur;
                return;
            }
        }

        cur->next = first;
        first = cur;
    }

    void rebalance(u32 priority) {
        Queue *cur = &queues[priority];
        int size = cur->end - cur->first;
        if (size >= cur->capacity - 2)  {
            IdType *new_data = (IdType *)realloc(cur->data, cur->capacity * 2 * sizeof(IdType));
            if (new_data != NULL)  {
                cur->capacity *= 2;
                cur->data = new_data;
            }
        }

        int newFirst = (cur->capacity - size) / 2;
        if (newFirst != cur->first) {
            memmove(&cur->data[newFirst], &cur->data[cur->first], size * sizeof(IdType));
            cur->first = newFirst;
            cur->end = newFirst + size;
        }
    }

    // The first queue that's ever been used.
    Queue *first;
    // The priority level queues of thread ids.
    Queue queues[NUM_QUEUES];
};

} // namespace
