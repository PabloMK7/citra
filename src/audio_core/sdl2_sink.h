// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include "audio_core/sink.h"

namespace AudioCore {

class SDL2Sink final : public Sink {
public:
    explicit SDL2Sink(std::string device_id);
    ~SDL2Sink() override;

    unsigned int GetNativeSampleRate() const override;

    void EnqueueSamples(const s16* samples, std::size_t sample_count) override;

    std::size_t SamplesInQueue() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

std::vector<std::string> ListSDL2SinkDevices();

} // namespace AudioCore
