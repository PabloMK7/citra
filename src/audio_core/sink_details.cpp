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
#ifdef HAVE_OPENAL
#include "audio_core/openal_sink.h"
#endif
#include "common/logging/log.h"

namespace AudioCore {
namespace {
struct SinkDetails {
    using FactoryFn = std::unique_ptr<Sink> (*)(std::string_view);
    using ListDevicesFn = std::vector<std::string> (*)();

    /// Type of this sink.
    SinkType type;
    /// Name for this sink.
    std::string_view name;
    /// A method to call to construct an instance of this type of sink.
    FactoryFn factory;
    /// A method to call to list available devices.
    ListDevicesFn list_devices;
};

// sink_details is ordered in terms of desirability, with the best choice at the top.
constexpr std::array sink_details = {
#ifdef HAVE_CUBEB
    SinkDetails{SinkType::Cubeb, "Cubeb",
                [](std::string_view device_id) -> std::unique_ptr<Sink> {
                    return std::make_unique<CubebSink>(device_id);
                },
                &ListCubebSinkDevices},
#endif
#ifdef HAVE_OPENAL
    SinkDetails{SinkType::OpenAL, "OpenAL",
                [](std::string_view device_id) -> std::unique_ptr<Sink> {
                    return std::make_unique<OpenALSink>(std::string(device_id));
                },
                &ListOpenALSinkDevices},
#endif
#ifdef HAVE_SDL2
    SinkDetails{SinkType::SDL2, "SDL2",
                [](std::string_view device_id) -> std::unique_ptr<Sink> {
                    return std::make_unique<SDL2Sink>(std::string(device_id));
                },
                &ListSDL2SinkDevices},
#endif
    SinkDetails{SinkType::Null, "None",
                [](std::string_view device_id) -> std::unique_ptr<Sink> {
                    return std::make_unique<NullSink>(device_id);
                },
                [] { return std::vector<std::string>{"None"}; }},
};

const SinkDetails& GetSinkDetails(SinkType sink_type) {
    auto iter = std::find_if(
        sink_details.begin(), sink_details.end(),
        [sink_type](const auto& sink_detail) { return sink_detail.type == sink_type; });

    if (sink_type == SinkType::Auto || iter == sink_details.end()) {
        if (sink_type != SinkType::Auto) {
            LOG_ERROR(Audio, "AudioCore::GetSinkDetails given invalid sink_type {}", sink_type);
        }
        // Auto-select.
        // sink_details is ordered in terms of desirability, with the best choice at the front.
        iter = sink_details.begin();
    }

    return *iter;
}
} // Anonymous namespace

std::string_view GetSinkName(SinkType sink_type) {
    if (sink_type == SinkType::Auto) {
        return "Auto";
    }
    return GetSinkDetails(sink_type).name;
}

std::vector<std::string> GetDeviceListForSink(SinkType sink_type) {
    return GetSinkDetails(sink_type).list_devices();
}

std::unique_ptr<Sink> CreateSinkFromID(SinkType sink_type, std::string_view device_id) {
    return GetSinkDetails(sink_type).factory(device_id);
}

} // namespace AudioCore
