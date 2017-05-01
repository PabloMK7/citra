// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include "common/common_types.h"
#include "core/memory.h"

namespace AudioCore {

constexpr int native_sample_rate = 32728; ///< 32kHz

/// Initialise Audio Core
void Init();

/// Returns a reference to the array backing DSP memory
std::array<u8, Memory::DSP_RAM_SIZE>& GetDspMemory();

/// Select the sink to use based on sink id.
void SelectSink(std::string sink_id);

/// Enable/Disable stretching.
void EnableStretching(bool enable);

/// Shutdown Audio Core
void Shutdown();

} // namespace AudioCore
