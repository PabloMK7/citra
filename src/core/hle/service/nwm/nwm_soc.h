// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace NWM {

class NWM_SOC final : public Interface {
public:
    NWM_SOC();

    std::string GetPortName() const override {
        return "nwm::SOC";
    }
};

} // namespace NWM
} // namespace Service
