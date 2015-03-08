// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>

#include "common/assert.h"

#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/logging/text_formatter.h"

namespace Log {

static std::shared_ptr<Logger> global_logger;

/// Macro listing all log classes. Code should define CLS and SUB as desired before invoking this.
#define ALL_LOG_CLASSES() \
        CLS(Log) \
        CLS(Common) \
        SUB(Common, Filesystem) \
        SUB(Common, Memory) \
        CLS(Core) \
        SUB(Core, ARM11) \
        SUB(Core, Timing) \
        CLS(Config) \
        CLS(Debug) \
        SUB(Debug, Emulated) \
        SUB(Debug, GPU) \
        SUB(Debug, Breakpoint) \
        CLS(Kernel) \
        SUB(Kernel, SVC) \
        CLS(Service) \
        SUB(Service, SRV) \
        SUB(Service, FS) \
        SUB(Service, ERR) \
        SUB(Service, APT) \
        SUB(Service, GSP) \
        SUB(Service, AC) \
        SUB(Service, PTM) \
        SUB(Service, LDR) \
        SUB(Service, NIM) \
        SUB(Service, NWM) \
        SUB(Service, CFG) \
        SUB(Service, DSP) \
        SUB(Service, HID) \
        SUB(Service, SOC) \
        CLS(HW) \
        SUB(HW, Memory) \
        SUB(HW, LCD) \
        SUB(HW, GPU) \
        CLS(Frontend) \
        CLS(Render) \
        SUB(Render, Software) \
        SUB(Render, OpenGL) \
        CLS(Loader)

Logger::Logger() {
    // Register logging classes so that they can be queried at runtime
    size_t parent_class;
    all_classes.reserve((size_t)Class::Count);

#define CLS(x) \
        all_classes.push_back(Class::x); \
        parent_class = all_classes.size() - 1;
#define SUB(x, y) \
        all_classes.push_back(Class::x##_##y); \
        all_classes[parent_class].num_children += 1;

    ALL_LOG_CLASSES()
#undef CLS
#undef SUB

    // Ensures that ALL_LOG_CLASSES isn't missing any entries.
    DEBUG_ASSERT(all_classes.size() == (size_t)Class::Count);
}

// GetClassName is a macro defined by Windows.h, grrr...
const char* Logger::GetLogClassName(Class log_class) {
    switch (log_class) {
#define CLS(x) case Class::x: return #x;
#define SUB(x, y) case Class::x##_##y: return #x "." #y;
        ALL_LOG_CLASSES()
#undef CLS
#undef SUB
    }
    return "Unknown";
}

const char* Logger::GetLevelName(Level log_level) {
#define LVL(x) case Level::x: return #x
    switch (log_level) {
        LVL(Trace);
        LVL(Debug);
        LVL(Info);
        LVL(Warning);
        LVL(Error);
        LVL(Critical);
    }
    return "Unknown";
#undef LVL
}

void Logger::LogMessage(Entry entry) {
    ring_buffer.Push(std::move(entry));
}

size_t Logger::GetEntries(Entry* out_buffer, size_t buffer_len) {
    return ring_buffer.BlockingPop(out_buffer, buffer_len);
}

std::shared_ptr<Logger> InitGlobalLogger() {
    global_logger = std::make_shared<Logger>();
    return global_logger;
}

Entry CreateEntry(Class log_class, Level log_level,
                        const char* filename, unsigned int line_nr, const char* function,
                        const char* format, va_list args) {
    using std::chrono::steady_clock;
    using std::chrono::duration_cast;

    static steady_clock::time_point time_origin = steady_clock::now();

    std::array<char, 4 * 1024> formatting_buffer;

    Entry entry;
    entry.timestamp = duration_cast<std::chrono::microseconds>(steady_clock::now() - time_origin);
    entry.log_class = log_class;
    entry.log_level = log_level;

    snprintf(formatting_buffer.data(), formatting_buffer.size(), "%s:%s:%u", filename, function, line_nr);
    entry.location = std::string(formatting_buffer.data());

    vsnprintf(formatting_buffer.data(), formatting_buffer.size(), format, args);
    entry.message = std::string(formatting_buffer.data());

    return std::move(entry);
}

static Filter* filter;

void SetFilter(Filter* new_filter) {
    filter = new_filter;
}

void LogMessage(Class log_class, Level log_level,
                const char* filename, unsigned int line_nr, const char* function,
                const char* format, ...) {
    if (!filter->CheckMessage(log_class, log_level))
        return;

    va_list args;
    va_start(args, format);
    Entry entry = CreateEntry(log_class, log_level,
            filename, line_nr, function, format, args);
    va_end(args);

    if (global_logger != nullptr && !global_logger->IsClosed()) {
        global_logger->LogMessage(std::move(entry));
    } else {
        // Fall back to directly printing to stderr
        PrintMessage(entry);
    }
}

}
