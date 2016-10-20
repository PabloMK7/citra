// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

namespace Log {

struct Entry;

/**
 * Attempts to trim an arbitrary prefix from `path`, leaving only the part starting at `root`. It's
 * intended to be used to strip a system-specific build directory from the `__FILE__` macro,
 * leaving only the path relative to the sources root.
 *
 * @param path The input file path as a null-terminated string
 * @param root The name of the root source directory as a null-terminated string. Path up to and
 *             including the last occurrence of this name will be stripped
 * @return A pointer to the same string passed as `path`, but starting at the trimmed portion
 */
const char* TrimSourcePath(const char* path, const char* root = "src");

/// Formats a log entry into the provided text buffer.
void FormatLogMessage(const Entry& entry, char* out_text, size_t text_len);
/// Formats and prints a log entry to stderr.
void PrintMessage(const Entry& entry);
/// Prints the same message as `PrintMessage`, but colored acoording to the severity level.
void PrintColoredMessage(const Entry& entry);
}
