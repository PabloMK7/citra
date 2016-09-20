// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include "audio_core/hle/common.h"
#include "audio_core/hle/dsp.h"

namespace DSP {
namespace HLE {

class Mixers final {
public:
    Mixers() {
        Reset();
    }

    void Reset();

    DspStatus Tick(DspConfiguration& config, const IntermediateMixSamples& read_samples,
                   IntermediateMixSamples& write_samples, const std::array<QuadFrame32, 3>& input);

    StereoFrame16 GetOutput() const {
        return current_frame;
    }

private:
    StereoFrame16 current_frame = {};

    using OutputFormat = DspConfiguration::OutputFormat;

    struct {
        std::array<float, 3> intermediate_mixer_volume = {};

        bool mixer1_enabled = false;
        bool mixer2_enabled = false;
        std::array<QuadFrame32, 3> intermediate_mix_buffer = {};

        OutputFormat output_format = OutputFormat::Stereo;

    } state;

    /// INTERNAL: Update our internal state based on the current config.
    void ParseConfig(DspConfiguration& config);
    /// INTERNAL: Read samples from shared memory that have been modified by the ARM11.
    void AuxReturn(const IntermediateMixSamples& read_samples);
    /// INTERNAL: Write samples to shared memory for the ARM11 to modify.
    void AuxSend(IntermediateMixSamples& write_samples, const std::array<QuadFrame32, 3>& input);
    /// INTERNAL: Mix current_frame.
    void MixCurrentFrame();
    /// INTERNAL: Downmix from quadraphonic to stereo based on status.output_format and accumulate
    /// into current_frame.
    void DownmixAndMixIntoCurrentFrame(float gain, const QuadFrame32& samples);
    /// INTERNAL: Generate DspStatus based on internal state.
    DspStatus GetCurrentStatus() const;
};

} // namespace HLE
} // namespace DSP
