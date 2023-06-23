// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <filesystem>
#include "common/logging/filter.h"

namespace Common::Log {

class Filter;

/// Initializes the logging system. This should be the first thing called in main.
void Initialize();

void DisableLoggingInTests();

/**
 * The global filter will prevent any messages from even being processed if they are filtered.
 */
void SetGlobalFilter(const Filter& filter);

void SetColorConsoleBackendEnabled(bool enabled);
} // namespace Common::Log
