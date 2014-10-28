// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2+
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>

namespace Log {

class Logger;
struct Entry;

/// Formats a log entry into the provided text buffer.
void FormatLogMessage(const Entry& entry, char* out_text, size_t text_len);
/// Formats and prints a log entry to stderr.
void PrintMessage(const Entry& entry);

/**
 * Logging loop that repeatedly reads messages from the provided logger and prints them to the
 * console. It is the baseline barebones log outputter.
 */
void TextLoggingLoop(std::shared_ptr<Logger> logger);

}
