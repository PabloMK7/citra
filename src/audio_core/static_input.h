// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "audio_core/input.h"
#include "common/swap.h"
#include "common/threadsafe_queue.h"

namespace AudioCore {

class StaticInput final : public Input {
public:
    StaticInput();
    ~StaticInput() override;

    void StartSampling(const InputParameters& params) override;
    void StopSampling() override;
    void AdjustSampleRate(u32 sample_rate) override;

    Samples Read() override;

private:
    u16 sample_rate = 0;
    u8 sample_size = 0;
    std::vector<u8> CACHE_8_BIT;
    std::vector<u8> CACHE_16_BIT;
};

} // namespace AudioCore
