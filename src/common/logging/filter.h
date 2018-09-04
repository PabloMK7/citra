// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <cstddef>
#include <string_view>
#include "common/logging/log.h"

namespace Log {

/**
 * Implements a log message filter which allows different log classes to have different minimum
 * severity levels. The filter can be changed at runtime and can be parsed from a string to allow
 * editing via the interface or loading from a configuration file.
 */
class Filter {
public:
    /// Initializes the filter with all classes having `default_level` as the minimum level.
    explicit Filter(Level default_level = Level::Info);

    /// Resets the filter so that all classes have `level` as the minimum displayed level.
    void ResetAll(Level level);
    /// Sets the minimum level of `log_class` (and not of its subclasses) to `level`.
    void SetClassLevel(Class log_class, Level level);

    /**
     * Parses a filter string and applies it to this filter.
     *
     * A filter string consists of a space-separated list of filter rules, each of the format
     * `<class>:<level>`. `<class>` is a log class name, with subclasses separated using periods.
     * `*` is allowed as a class name and will reset all filters to the specified level. `<level>`
     * a severity level name which will be set as the minimum logging level of the matched classes.
     * Rules are applied left to right, with each rule overriding previous ones in the sequence.
     *
     * A few examples of filter rules:
     *  - `*:Info` -- Resets the level of all classes to Info.
     *  - `Service:Info` -- Sets the level of Service to Info.
     *  - `Service.FS:Trace` -- Sets the level of the Service.FS class to Trace.
     */
    void ParseFilterString(std::string_view filter_view);

    /// Matches class/level combination against the filter, returning true if it passed.
    bool CheckMessage(Class log_class, Level level) const;

private:
    std::array<Level, static_cast<std::size_t>(Class::Count)> class_levels;
};
} // namespace Log
