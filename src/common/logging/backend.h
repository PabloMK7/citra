// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstdarg>
#include <memory>
#include <vector>

#include "common/concurrent_ring_buffer.h"

#include "common/logging/log.h"

namespace Log {

/**
 * A log entry. Log entries are store in a structured format to permit more varied output
 * formatting on different frontends, as well as facilitating filtering and aggregation.
 */
struct Entry {
    std::chrono::microseconds timestamp;
    Class log_class;
    Level log_level;
    std::string location;
    std::string message;

    Entry() = default;

    // TODO(yuriks) Use defaulted move constructors once MSVC supports them
#define MOVE(member) member(std::move(o.member))
    Entry(Entry&& o)
        : MOVE(timestamp), MOVE(log_class), MOVE(log_level),
        MOVE(location), MOVE(message)
    {}
#undef MOVE

    Entry& operator=(const Entry&& o) {
#define MOVE(member) member = std::move(o.member)
        MOVE(timestamp);
        MOVE(log_class);
        MOVE(log_level);
        MOVE(location);
        MOVE(message);
#undef MOVE
        return *this;
    }
};

struct ClassInfo {
    Class log_class;

    /**
        * Total number of (direct or indirect) sub classes this class has. If any, they follow in
        * sequence after this class in the class list.
        */
    unsigned int num_children = 0;

    ClassInfo(Class log_class) : log_class(log_class) {}
};

/**
 * Logging management class. This class has the dual purpose of acting as an exchange point between
 * the logging clients and the log outputter, as well as containing reflection info about available
 * log classes.
 */
class Logger {
private:
    using Buffer = Common::ConcurrentRingBuffer<Entry, 16 * 1024 / sizeof(Entry)>;

public:
    static const size_t QUEUE_CLOSED = Buffer::QUEUE_CLOSED;

    Logger();

    /**
     * Returns a list of all vector classes and subclasses. The sequence returned is a pre-order of
     * classes and subclasses, which together with the `num_children` field in ClassInfo, allows
     * you to recover the hierarchy.
     */
    const std::vector<ClassInfo>& GetClasses() const { return all_classes; }

    /**
     * Returns the name of the passed log class as a C-string. Subclasses are separated by periods
     * instead of underscores as in the enumeration.
     */
    static const char* GetLogClassName(Class log_class);

    /**
     * Returns the name of the passed log level as a C-string.
     */
    static const char* GetLevelName(Level log_level);

    /**
     * Appends a messages to the log buffer.
     * @note This function is thread safe.
     */
    void LogMessage(Entry entry);

    /**
     * Retrieves a batch of messages from the log buffer, blocking until they are available.
     * @note This function is thread safe.
     *
     * @param out_buffer Destination buffer that will receive the log entries.
     * @param buffer_len The maximum size of `out_buffer`.
     * @return The number of entries stored. In case the logger is shutting down, `QUEUE_CLOSED` is
     *         returned, no entries are stored and the logger should shutdown.
     */
    size_t GetEntries(Entry* out_buffer, size_t buffer_len);

    /**
     * Initiates a shutdown of the logger. This will indicate to log output clients that they
     * should shutdown.
     */
    void Close() { ring_buffer.Close(); }

    /**
     * Returns true if Close() has already been called on the Logger.
     */
    bool IsClosed() const { return ring_buffer.IsClosed(); }

private:
    Buffer ring_buffer;
    std::vector<ClassInfo> all_classes;
};

/// Creates a log entry by formatting the given source location, and message.
Entry CreateEntry(Class log_class, Level log_level,
                        const char* filename, unsigned int line_nr, const char* function,
                        const char* format, va_list args);
/// Initializes the default Logger.
std::shared_ptr<Logger> InitGlobalLogger();

}
