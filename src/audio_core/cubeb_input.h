// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <vector>
#include "core/frontend/mic.h"

namespace AudioCore {

class CubebInput final : public Frontend::Mic::Interface {
public:
    explicit CubebInput(std::string device_id);
    ~CubebInput() override;

    void StartSampling(const Frontend::Mic::Parameters& params) override;

    void StopSampling() override;

    void AdjustSampleRate(u32 sample_rate) override;

    Frontend::Mic::Samples Read() override;

private:
    struct Impl;
    std::unique_ptr<Impl> impl;
    std::string device_id;
};

std::vector<std::string> ListCubebInputDevices();

} // namespace AudioCore
