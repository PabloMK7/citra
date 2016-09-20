// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include "common/common_types.h"

namespace AudioCore {

/**
 * This class is an interface for an audio sink. An audio sink accepts samples in stereo signed
 * PCM16 format to be output. Sinks *do not* handle resampling and expect the correct sample rate.
 * They are dumb outputs.
 */
class Sink {
public:
    virtual ~Sink() = default;

    /// The native rate of this sink. The sink expects to be fed samples that respect this. (Units:
    /// samples/sec)
    virtual unsigned int GetNativeSampleRate() const = 0;

    /**
     * Feed stereo samples to sink.
     * @param samples Samples in interleaved stereo PCM16 format.
     * @param sample_count Number of samples.
     */
    virtual void EnqueueSamples(const s16* samples, size_t sample_count) = 0;

    /// Samples enqueued that have not been played yet.
    virtual std::size_t SamplesInQueue() const = 0;
};

} // namespace
