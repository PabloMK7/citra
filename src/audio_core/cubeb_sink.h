// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <cstddef>
#include <memory>
#include "audio_core/sink.h"

namespace AudioCore {

class CubebSink final : public Sink {
public:
    explicit CubebSink(std::string device_id);
    ~CubebSink() override;

    unsigned int GetNativeSampleRate() const override;

    void EnqueueSamples(const s16* samples, std::size_t sample_count) override;

    std::size_t SamplesInQueue() const override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
};

std::vector<std::string> ListCubebSinkDevices();

} // namespace AudioCore
