// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <array>
#include <memory>

#include "audio_core/hle/dsp.h"
#include "audio_core/hle/mixers.h"
#include "audio_core/hle/pipe.h"
#include "audio_core/hle/source.h"
#include "audio_core/sink.h"
#include "audio_core/time_stretch.h"

namespace DSP {
namespace HLE {

// Region management

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

// Audio processing and mixing

static std::array<Source, num_sources> sources = {
    Source(0), Source(1), Source(2), Source(3), Source(4), Source(5),
    Source(6), Source(7), Source(8), Source(9), Source(10), Source(11),
    Source(12), Source(13), Source(14), Source(15), Source(16), Source(17),
    Source(18), Source(19), Source(20), Source(21), Source(22), Source(23)
};
static Mixers mixers;

static StereoFrame16 GenerateCurrentFrame() {
    SharedMemory& read = ReadRegion();
    SharedMemory& write = WriteRegion();

    std::array<QuadFrame32, 3> intermediate_mixes = {};

    // Generate intermediate mixes
    for (size_t i = 0; i < num_sources; i++) {
        write.source_statuses.status[i] = sources[i].Tick(read.source_configurations.config[i], read.adpcm_coefficients.coeff[i]);
        for (size_t mix = 0; mix < 3; mix++) {
            sources[i].MixInto(intermediate_mixes[mix], mix);
        }
    }

    // Generate final mix
    write.dsp_status = mixers.Tick(read.dsp_configuration, read.intermediate_mix_samples, write.intermediate_mix_samples, intermediate_mixes);

    StereoFrame16 output_frame = mixers.GetOutput();

    // Write current output frame to the shared memory region
    for (size_t samplei = 0; samplei < output_frame.size(); samplei++) {
        for (size_t channeli = 0; channeli < output_frame[0].size(); channeli++) {
            write.final_samples.pcm16[samplei][channeli] = s16_le(output_frame[samplei][channeli]);
        }
    }

    return output_frame;
}

// Audio output

static bool perform_time_stretching = true;
static std::unique_ptr<AudioCore::Sink> sink;
static AudioCore::TimeStretcher time_stretcher;

static void FlushResidualStretcherAudio() {
    time_stretcher.Flush();
    while (true) {
        std::vector<s16> residual_audio = time_stretcher.Process(sink->SamplesInQueue());
        if (residual_audio.empty())
            break;
        sink->EnqueueSamples(residual_audio.data(), residual_audio.size() / 2);
    }
}

static void OutputCurrentFrame(const StereoFrame16& frame) {
    if (perform_time_stretching) {
        time_stretcher.AddSamples(&frame[0][0], frame.size());
        std::vector<s16> stretched_samples = time_stretcher.Process(sink->SamplesInQueue());
        sink->EnqueueSamples(stretched_samples.data(), stretched_samples.size() / 2);
    } else {
        constexpr size_t maximum_sample_latency = 1024; // about 32 miliseconds
        if (sink->SamplesInQueue() > maximum_sample_latency) {
            // This can occur if we're running too fast and samples are starting to back up.
            // Just drop the samples.
            return;
        }

        sink->EnqueueSamples(&frame[0][0], frame.size());
    }
}

void EnableStretching(bool enable) {
    if (perform_time_stretching == enable)
        return;

    if (!enable) {
        FlushResidualStretcherAudio();
    }
    perform_time_stretching = enable;
}

// Public Interface

void Init() {
    DSP::HLE::ResetPipes();

    for (auto& source : sources) {
        source.Reset();
    }

    mixers.Reset();

    time_stretcher.Reset();
    if (sink) {
        time_stretcher.SetOutputSampleRate(sink->GetNativeSampleRate());
    }
}

void Shutdown() {
    if (perform_time_stretching) {
        FlushResidualStretcherAudio();
    }
}

bool Tick() {
    StereoFrame16 current_frame = {};

    // TODO: Check dsp::DSP semaphore (which indicates emulated application has finished writing to shared memory region)
    current_frame = GenerateCurrentFrame();

    OutputCurrentFrame(current_frame);

    return true;
}

void SetSink(std::unique_ptr<AudioCore::Sink> sink_) {
    sink = std::move(sink_);
    time_stretcher.SetOutputSampleRate(sink->GetNativeSampleRate());
}

} // namespace HLE
} // namespace DSP
