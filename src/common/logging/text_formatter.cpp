// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#include <array>
#include <cstdio>

#include "common/logging/backend.h"
#include "common/logging/log.h"
#include "common/logging/text_formatter.h"

namespace Log {

void FormatLogMessage(const Entry& entry, char* out_text, size_t text_len) {
    unsigned int time_seconds    = static_cast<unsigned int>(entry.timestamp.count() / 1000000);
    unsigned int time_fractional = static_cast<unsigned int>(entry.timestamp.count() % 1000000);

    const char* class_name = Logger::GetLogClassName(entry.log_class);
    const char* level_name = Logger::GetLevelName(entry.log_level);

    snprintf(out_text, text_len, "[%4u.%06u] %s <%s> %s: %s",
        time_seconds, time_fractional, class_name, level_name,
        entry.location.c_str(), entry.message.c_str());
}

void PrintMessage(const Entry& entry) {
    std::array<char, 4 * 1024> format_buffer;
    FormatLogMessage(entry, format_buffer.data(), format_buffer.size());
    fputs(format_buffer.data(), stderr);
    fputc('\n', stderr);
}

void TextLoggingLoop(std::shared_ptr<Logger> logger) {
    std::array<Entry, 256> entry_buffer;

    while (true) {
        size_t num_entries = logger->GetEntries(entry_buffer.data(), entry_buffer.size());
        if (num_entries == Logger::QUEUE_CLOSED) {
            break;
        }
        for (size_t i = 0; i < num_entries; ++i) {
            PrintMessage(entry_buffer[i]);
        }
    }
}

}
