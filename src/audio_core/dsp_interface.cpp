// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstddef>
#include "audio_core/dsp_interface.h"
#include "audio_core/sink.h"
#include "audio_core/sink_details.h"
#include "common/assert.h"
#include "core/settings.h"

namespace AudioCore {

DspInterface::DspInterface() = default;

DspInterface::~DspInterface() {
    if (perform_time_stretching) {
        FlushResidualStretcherAudio();
    }
}

void DspInterface::SetSink(const std::string& sink_id, const std::string& audio_device) {
    const SinkDetails& sink_details = GetSinkDetails(sink_id);
    sink = sink_details.factory(audio_device);
    time_stretcher.SetOutputSampleRate(sink->GetNativeSampleRate());
}

Sink& DspInterface::GetSink() {
    ASSERT(sink);
    return *sink.get();
}

void DspInterface::EnableStretching(bool enable) {
    if (perform_time_stretching == enable)
        return;

    if (!enable) {
        FlushResidualStretcherAudio();
    }
    perform_time_stretching = enable;
}

void DspInterface::OutputFrame(StereoFrame16& frame) {
    if (!sink)
        return;

    // Implementation of the hardware volume slider with a dynamic range of 60 dB
    double volume_scale_factor = std::exp(6.90775 * Settings::values.volume) * 0.001;
    for (std::size_t i = 0; i < frame.size(); i++) {
        frame[i][0] = static_cast<s16>(frame[i][0] * volume_scale_factor);
        frame[i][1] = static_cast<s16>(frame[i][1] * volume_scale_factor);
    }

    if (perform_time_stretching) {
        time_stretcher.AddSamples(&frame[0][0], frame.size());
        std::vector<s16> stretched_samples = time_stretcher.Process(sink->SamplesInQueue());
        sink->EnqueueSamples(stretched_samples.data(), stretched_samples.size() / 2);
    } else {
        constexpr std::size_t maximum_sample_latency = 2048; // about 64 miliseconds
        if (sink->SamplesInQueue() > maximum_sample_latency) {
            // This can occur if we're running too fast and samples are starting to back up.
            // Just drop the samples.
            return;
        }

        sink->EnqueueSamples(&frame[0][0], frame.size());
    }
}

void DspInterface::FlushResidualStretcherAudio() {
    if (!sink)
        return;

    time_stretcher.Flush();
    while (true) {
        std::vector<s16> residual_audio = time_stretcher.Process(sink->SamplesInQueue());
        if (residual_audio.empty())
            break;
        sink->EnqueueSamples(residual_audio.data(), residual_audio.size() / 2);
    }
}

} // namespace AudioCore
