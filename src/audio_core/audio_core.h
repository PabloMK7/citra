// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

namespace Kernel {
class VMManager;
}

namespace AudioCore {

constexpr int native_sample_rate = 32728;  ///< 32kHz

/// Initialise Audio Core
void Init();

/// Add DSP address spaces to a Process.
void AddAddressSpace(Kernel::VMManager& vm_manager);

/// Select the sink to use based on sink id.
void SelectSink(std::string sink_id);

/// Enable/Disable stretching.
void EnableStretching(bool enable);

/// Shutdown Audio Core
void Shutdown();

} // namespace
