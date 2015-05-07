// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <condition_variable>
#include <cstdint>
#include <mutex>
#include <thread>

#include "common/common_types.h" // for NonCopyable

namespace Common {

/**
 * A MPMC (Multiple-Producer Multiple-Consumer) concurrent ring buffer. This data structure permits
 * multiple threads to push and pop from a queue of bounded size.
 */
template <typename T, size_t ArraySize>
class ConcurrentRingBuffer : private NonCopyable {
public:
    /// Value returned by the popping functions when the queue has been closed.
    static const size_t QUEUE_CLOSED = -1;

    ConcurrentRingBuffer() {}

    ~ConcurrentRingBuffer() {
        // If for whatever reason the queue wasn't completely drained, destroy the left over items.
        for (size_t i = reader_index, end = writer_index; i != end; i = (i + 1) % ArraySize) {
            Data()[i].~T();
        }
    }

    /**
     * Pushes a value to the queue. If the queue is full, this method will block. Does nothing if
     * the queue is closed.
     */
    void Push(T val) {
        std::unique_lock<std::mutex> lock(mutex);
        if (closed) {
            return;
        }

        // If the buffer is full, wait
        writer.wait(lock, [&]{
            return (writer_index + 1) % ArraySize != reader_index;
        });

        T* item = &Data()[writer_index];
        new (item) T(std::move(val));

        writer_index = (writer_index + 1) % ArraySize;

        // Wake up waiting readers
        lock.unlock();
        reader.notify_one();
    }

    /**
     * Pops up to `dest_len` items from the queue, storing them in `dest`. This function will not
     * block, and might return 0 values if there are no elements in the queue when it is called.
     *
     * @return The number of elements stored in `dest`. If the queue has been closed, returns
     *          `QUEUE_CLOSED`.
     */
    size_t Pop(T* dest, size_t dest_len) {
        std::unique_lock<std::mutex> lock(mutex);
        if (closed && !CanRead()) {
            return QUEUE_CLOSED;
        }
        return PopInternal(dest, dest_len);
    }

    /**
     * Pops up to `dest_len` items from the queue, storing them in `dest`. This function will block
     * if there are no elements in the queue when it is called.
     *
     * @return The number of elements stored in `dest`. If the queue has been closed, returns
     *         `QUEUE_CLOSED`.
     */
    size_t BlockingPop(T* dest, size_t dest_len) {
        std::unique_lock<std::mutex> lock(mutex);
        if (closed && !CanRead()) {
            return QUEUE_CLOSED;
        }

        while (!CanRead()) {
            reader.wait(lock);
            if (closed && !CanRead()) {
                return QUEUE_CLOSED;
            }
        }
        DEBUG_ASSERT(CanRead());
        return PopInternal(dest, dest_len);
    }

    /**
     * Closes the queue. After calling this method, `Push` operations won't have any effect, and
     * `PopMany` and `PopManyBlock` will start returning `QUEUE_CLOSED`. This is intended to allow
     * a graceful shutdown of all consumers.
     */
    void Close() {
        std::unique_lock<std::mutex> lock(mutex);
        closed = true;
        // We need to wake up any reader that are waiting for an item that will never come.
        lock.unlock();
        reader.notify_all();
    }

    /// Returns true if `Close()` has been called.
    bool IsClosed() const {
        return closed;
    }

private:
    size_t PopInternal(T* dest, size_t dest_len) {
        size_t output_count = 0;
        while (output_count < dest_len && CanRead()) {
            DEBUG_ASSERT(CanRead());

            T* item = &Data()[reader_index];
            T out_val = std::move(*item);
            item->~T();

            size_t prev_index = (reader_index + ArraySize - 1) % ArraySize;
            reader_index = (reader_index + 1) % ArraySize;
            if (writer_index == prev_index) {
                writer.notify_one();
            }
            dest[output_count++] = std::move(out_val);
        }
        return output_count;
    }

    bool CanRead() const {
        return reader_index != writer_index;
    }

    T* Data() {
        return static_cast<T*>(static_cast<void*>(&storage));
    }

    /// Storage for entries
    typename std::aligned_storage<ArraySize * sizeof(T),
                                  std::alignment_of<T>::value>::type storage;

    /// Data is valid in the half-open interval [reader, writer). If they are `QUEUE_CLOSED` then the
    /// queue has been closed.
    size_t writer_index = 0, reader_index = 0;
    // True if the queue has been closed.
    bool closed = false;

    /// Mutex that protects the entire data structure.
    std::mutex mutex;
    /// Signaling wakes up reader which is waiting for storage to be non-empty.
    std::condition_variable reader;
    /// Signaling wakes up writer which is waiting for storage to be non-full.
    std::condition_variable writer;
};

} // namespace
