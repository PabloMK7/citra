// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/hle/dsp.h"
#include "audio_core/hle/pipe.h"

namespace DSP {
namespace HLE {

SharedMemory g_region0;
SharedMemory g_region1;

void Init() {
    DSP::HLE::ResetPipes();
}

void Shutdown() {
}

bool Tick() {
    return true;
}

SharedMemory& CurrentRegion() {
    // The region with the higher frame counter is chosen unless there is wraparound.

    if (g_region0.frame_counter == 0xFFFFu && g_region1.frame_counter != 0xFFFEu) {
        // Wraparound has occured.
        return g_region1;
    }

    if (g_region1.frame_counter == 0xFFFFu && g_region0.frame_counter != 0xFFFEu) {
        // Wraparound has occured.
        return g_region0;
    }

    return (g_region0.frame_counter > g_region1.frame_counter) ? g_region0 : g_region1;
}

} // namespace HLE
} // namespace DSP
