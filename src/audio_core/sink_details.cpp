// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <string>
#include <vector>
#include "audio_core/null_sink.h"
#include "audio_core/sink_details.h"
#ifdef HAVE_SDL2
#include "audio_core/sdl2_sink.h"
#endif
#include "common/logging/log.h"

namespace AudioCore {

// g_sink_details is ordered in terms of desirability, with the best choice at the top.
const std::vector<SinkDetails> g_sink_details = {
#ifdef HAVE_SDL2
    {"sdl2", []() { return std::make_unique<SDL2Sink>(); }},
#endif
    {"null", []() { return std::make_unique<NullSink>(); }},
};

const SinkDetails& GetSinkDetails(std::string sink_id) {
    auto iter =
        std::find_if(g_sink_details.begin(), g_sink_details.end(),
                     [sink_id](const auto& sink_detail) { return sink_detail.id == sink_id; });

    if (sink_id == "auto" || iter == g_sink_details.end()) {
        if (sink_id != "auto") {
            LOG_ERROR(Audio, "AudioCore::SelectSink given invalid sink_id %s", sink_id.c_str());
        }
        // Auto-select.
        // g_sink_details is ordered in terms of desirability, with the best choice at the front.
        iter = g_sink_details.begin();
    }

    return *iter;
}

} // namespace AudioCore
