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
namespace {
struct SinkDetails {
    using FactoryFn = std::unique_ptr<Sink> (*)(std::string_view);
    using ListDevicesFn = std::vector<std::string> (*)();

    /// Name for this sink.
    const char* id;
    /// A method to call to construct an instance of this type of sink.
    FactoryFn factory;
    /// A method to call to list available devices.
    ListDevicesFn list_devices;
};

// sink_details is ordered in terms of desirability, with the best choice at the top.
constexpr SinkDetails sink_details[] = {
#ifdef HAVE_CUBEB
    SinkDetails{"cubeb",
                [](std::string_view device_id) -> std::unique_ptr<Sink> {
                    return std::make_unique<CubebSink>(device_id);
                },
                &ListCubebSinkDevices},
#endif
#ifdef HAVE_SDL2
    SinkDetails{"sdl2",
                [](std::string_view device_id) -> std::unique_ptr<Sink> {
                    return std::make_unique<SDL2Sink>(std::string(device_id));
                },
                &ListSDL2SinkDevices},
#endif
    SinkDetails{"null",
                [](std::string_view device_id) -> std::unique_ptr<Sink> {
                    return std::make_unique<NullSink>(device_id);
                },
                [] { return std::vector<std::string>{"null"}; }},
};

const SinkDetails& GetSinkDetails(std::string_view sink_id) {
    auto iter =
        std::find_if(std::begin(sink_details), std::end(sink_details),
                     [sink_id](const auto& sink_detail) { return sink_detail.id == sink_id; });

    if (sink_id == "auto" || iter == std::end(sink_details)) {
        if (sink_id != "auto") {
            LOG_ERROR(Audio, "AudioCore::SelectSink given invalid sink_id {}", sink_id);
        }
        // Auto-select.
        // sink_details is ordered in terms of desirability, with the best choice at the front.
        iter = std::begin(sink_details);
    }

    return *iter;
}
} // Anonymous namespace

std::vector<const char*> GetSinkIDs() {
    std::vector<const char*> sink_ids(std::size(sink_details));

    std::transform(std::begin(sink_details), std::end(sink_details), std::begin(sink_ids),
                   [](const auto& sink) { return sink.id; });

    return sink_ids;
}

std::vector<std::string> GetDeviceListForSink(std::string_view sink_id) {
    return GetSinkDetails(sink_id).list_devices();
}

std::unique_ptr<Sink> CreateSinkFromID(std::string_view sink_id, std::string_view device_id) {
    return GetSinkDetails(sink_id).factory(device_id);
}

} // namespace AudioCore
