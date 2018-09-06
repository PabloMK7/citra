// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include <vector>
#include "common/common_types.h"

namespace AudioCore {

class TimeStretcher final {
public:
    TimeStretcher();
    ~TimeStretcher();

    /**
     * Set sample rate for the samples that Process returns.
     * @param sample_rate The sample rate.
     */
    void SetOutputSampleRate(unsigned int sample_rate);

    /**
     * Add samples to be processed.
     * @param sample_buffer Buffer of samples in interleaved stereo PCM16 format.
     * @param num_samples Number of samples.
     */
    void AddSamples(const s16* sample_buffer, std::size_t num_samples);

    /// Flush audio remaining in internal buffers.
    void Flush();

    /// Resets internal state and clears buffers.
    void Reset();

    /**
     * Does audio stretching and produces the time-stretched samples.
     * Timer calculations use sample_delay to determine how much of a margin we have.
     * @param sample_delay How many samples are buffered downstream of this module and haven't been
     * played yet.
     * @return Samples to play in interleaved stereo PCM16 format.
     */
    std::vector<s16> Process(std::size_t sample_delay);

private:
    struct Impl;
    std::unique_ptr<Impl> impl;

    /// INTERNAL: ratio = wallclock time / emulated time
    double CalculateCurrentRatio();
    /// INTERNAL: If we have too many or too few samples downstream, nudge ratio in the appropriate
    /// direction.
    double CorrectForUnderAndOverflow(double ratio, std::size_t sample_delay) const;
    /// INTERNAL: Gets the time-stretched samples from SoundTouch.
    std::vector<s16> GetSamples();
};

} // namespace AudioCore
