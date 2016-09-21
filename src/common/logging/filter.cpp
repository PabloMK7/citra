// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/string_util.h"

namespace Log {

Filter::Filter(Level default_level) {
    ResetAll(default_level);
}

void Filter::ResetAll(Level level) {
    class_levels.fill(level);
}

void Filter::SetClassLevel(Class log_class, Level level) {
    class_levels[static_cast<size_t>(log_class)] = level;
}

void Filter::ParseFilterString(const std::string& filter_str) {
    auto clause_begin = filter_str.cbegin();
    while (clause_begin != filter_str.cend()) {
        auto clause_end = std::find(clause_begin, filter_str.cend(), ' ');

        // If clause isn't empty
        if (clause_end != clause_begin) {
            ParseFilterRule(clause_begin, clause_end);
        }

        if (clause_end != filter_str.cend()) {
            // Skip over the whitespace
            ++clause_end;
        }
        clause_begin = clause_end;
    }
}

template <typename It>
static Level GetLevelByName(const It begin, const It end) {
    for (u8 i = 0; i < static_cast<u8>(Level::Count); ++i) {
        const char* level_name = GetLevelName(static_cast<Level>(i));
        if (Common::ComparePartialString(begin, end, level_name)) {
            return static_cast<Level>(i);
        }
    }
    return Level::Count;
}

template <typename It>
static Class GetClassByName(const It begin, const It end) {
    for (ClassType i = 0; i < static_cast<ClassType>(Class::Count); ++i) {
        const char* level_name = GetLogClassName(static_cast<Class>(i));
        if (Common::ComparePartialString(begin, end, level_name)) {
            return static_cast<Class>(i);
        }
    }
    return Class::Count;
}

bool Filter::ParseFilterRule(const std::string::const_iterator begin,
                             const std::string::const_iterator end) {
    auto level_separator = std::find(begin, end, ':');
    if (level_separator == end) {
        LOG_ERROR(Log, "Invalid log filter. Must specify a log level after `:`: %s",
                  std::string(begin, end).c_str());
        return false;
    }

    const Level level = GetLevelByName(level_separator + 1, end);
    if (level == Level::Count) {
        LOG_ERROR(Log, "Unknown log level in filter: %s", std::string(begin, end).c_str());
        return false;
    }

    if (Common::ComparePartialString(begin, level_separator, "*")) {
        ResetAll(level);
        return true;
    }

    const Class log_class = GetClassByName(begin, level_separator);
    if (log_class == Class::Count) {
        LOG_ERROR(Log, "Unknown log class in filter: %s", std::string(begin, end).c_str());
        return false;
    }

    SetClassLevel(log_class, level);
    return true;
}

bool Filter::CheckMessage(Class log_class, Level level) const {
    return static_cast<u8>(level) >= static_cast<u8>(class_levels[static_cast<size_t>(log_class)]);
}
}
