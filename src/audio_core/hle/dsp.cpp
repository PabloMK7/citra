// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "audio_core/hle/dsp.h"
#include "audio_core/hle/pipe.h"
#include "audio_core/sink.h"

namespace DSP {
namespace HLE {

std::array<SharedMemory, 2> g_regions;

static size_t CurrentRegionIndex() {
    // The region with the higher frame counter is chosen unless there is wraparound.
    // This function only returns a 0 or 1.

    if (g_regions[0].frame_counter == 0xFFFFu && g_regions[1].frame_counter != 0xFFFEu) {
        // Wraparound has occured.
        return 1;
    }

    if (g_regions[1].frame_counter == 0xFFFFu && g_regions[0].frame_counter != 0xFFFEu) {
        // Wraparound has occured.
        return 0;
    }

    return (g_regions[0].frame_counter > g_regions[1].frame_counter) ? 0 : 1;
}

static SharedMemory& ReadRegion() {
    return g_regions[CurrentRegionIndex()];
}

static SharedMemory& WriteRegion() {
    return g_regions[1 - CurrentRegionIndex()];
}

static std::unique_ptr<AudioCore::Sink> sink;

void Init() {
    DSP::HLE::ResetPipes();
}

void Shutdown() {
}

bool Tick() {
    return true;
}

void SetSink(std::unique_ptr<AudioCore::Sink> sink_) {
    sink = std::move(sink_);
}

} // namespace HLE
} // namespace DSP
