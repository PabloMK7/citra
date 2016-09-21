// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <vector>
#include "audio_core/null_sink.h"
#include "audio_core/sink_details.h"
#ifdef HAVE_SDL2
#include "audio_core/sdl2_sink.h"
#endif

namespace AudioCore {

// g_sink_details is ordered in terms of desirability, with the best choice at the top.
const std::vector<SinkDetails> g_sink_details = {
#ifdef HAVE_SDL2
    {"sdl2", []() { return std::make_unique<SDL2Sink>(); }},
#endif
    {"null", []() { return std::make_unique<NullSink>(); }},
};

} // namespace AudioCore
