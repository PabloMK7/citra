// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/frontend/input.h"

namespace InputCommon {
namespace SDL {

/// Initializes and registers SDL device factories
void Init();

/// Unresisters SDL device factories and shut them down.
void Shutdown();

} // namespace SDL
} // namespace InputCommon
