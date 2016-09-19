// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>

#include "audio_core/audio_core.h"
#include "audio_core/sink.h"

namespace AudioCore {

class NullSink final : public Sink {
public:
    ~NullSink() override = default;

    unsigned int GetNativeSampleRate() const override {
        return native_sample_rate;
    }

    void EnqueueSamples(const s16*, size_t) override {}

    size_t SamplesInQueue() const override {
        return 0;
    }
};

} // namespace AudioCore
