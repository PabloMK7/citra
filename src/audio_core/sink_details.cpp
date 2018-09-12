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
#ifdef HAVE_CUBEB
#include "audio_core/cubeb_sink.h"
#endif
#include "common/logging/log.h"

namespace AudioCore {

// g_sink_details is ordered in terms of desirability, with the best choice at the top.
const std::vector<SinkDetails> g_sink_details = {
#ifdef HAVE_CUBEB
    SinkDetails{"cubeb", &std::make_unique<CubebSink, std::string>, &ListCubebSinkDevices},
#endif
#ifdef HAVE_SDL2
    SinkDetails{"sdl2", &std::make_unique<SDL2Sink, std::string>, &ListSDL2SinkDevices},
#endif
    SinkDetails{"null", &std::make_unique<NullSink, std::string>,
                [] { return std::vector<std::string>{"null"}; }},
};

const SinkDetails& GetSinkDetails(std::string_view sink_id) {
    auto iter =
        std::find_if(g_sink_details.begin(), g_sink_details.end(),
                     [sink_id](const auto& sink_detail) { return sink_detail.id == sink_id; });

    if (sink_id == "auto" || iter == g_sink_details.end()) {
        if (sink_id != "auto") {
            LOG_ERROR(Audio, "AudioCore::SelectSink given invalid sink_id {}", sink_id);
        }
        // Auto-select.
        // g_sink_details is ordered in terms of desirability, with the best choice at the front.
        iter = g_sink_details.begin();
    }

    return *iter;
}

} // namespace AudioCore
