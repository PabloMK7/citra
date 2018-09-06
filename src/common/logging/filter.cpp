// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/string_util.h"

namespace Log {
namespace {
template <typename It>
Level GetLevelByName(const It begin, const It end) {
    for (u8 i = 0; i < static_cast<u8>(Level::Count); ++i) {
        const char* level_name = GetLevelName(static_cast<Level>(i));
        if (Common::ComparePartialString(begin, end, level_name)) {
            return static_cast<Level>(i);
        }
    }
    return Level::Count;
}

template <typename It>
Class GetClassByName(const It begin, const It end) {
    for (ClassType i = 0; i < static_cast<ClassType>(Class::Count); ++i) {
        const char* level_name = GetLogClassName(static_cast<Class>(i));
        if (Common::ComparePartialString(begin, end, level_name)) {
            return static_cast<Class>(i);
        }
    }
    return Class::Count;
}

template <typename Iterator>
bool ParseFilterRule(Filter& instance, Iterator begin, Iterator end) {
    auto level_separator = std::find(begin, end, ':');
    if (level_separator == end) {
        LOG_ERROR(Log, "Invalid log filter. Must specify a log level after `:`: {}",
                  std::string(begin, end));
        return false;
    }

    const Level level = GetLevelByName(level_separator + 1, end);
    if (level == Level::Count) {
        LOG_ERROR(Log, "Unknown log level in filter: {}", std::string(begin, end));
        return false;
    }

    if (Common::ComparePartialString(begin, level_separator, "*")) {
        instance.ResetAll(level);
        return true;
    }

    const Class log_class = GetClassByName(begin, level_separator);
    if (log_class == Class::Count) {
        LOG_ERROR(Log, "Unknown log class in filter: {}", std::string(begin, end));
        return false;
    }

    instance.SetClassLevel(log_class, level);
    return true;
}
} // Anonymous namespace

Filter::Filter(Level default_level) {
    ResetAll(default_level);
}

void Filter::ResetAll(Level level) {
    class_levels.fill(level);
}

void Filter::SetClassLevel(Class log_class, Level level) {
    class_levels[static_cast<std::size_t>(log_class)] = level;
}

void Filter::ParseFilterString(std::string_view filter_view) {
    auto clause_begin = filter_view.cbegin();
    while (clause_begin != filter_view.cend()) {
        auto clause_end = std::find(clause_begin, filter_view.cend(), ' ');

        // If clause isn't empty
        if (clause_end != clause_begin) {
            ParseFilterRule(*this, clause_begin, clause_end);
        }

        if (clause_end != filter_view.cend()) {
            // Skip over the whitespace
            ++clause_end;
        }
        clause_begin = clause_end;
    }
}

bool Filter::CheckMessage(Class log_class, Level level) const {
    return static_cast<u8>(level) >=
           static_cast<u8>(class_levels[static_cast<std::size_t>(log_class)]);
}
} // namespace Log
