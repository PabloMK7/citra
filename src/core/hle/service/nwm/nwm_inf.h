// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace NWM {

class NWM_INF final : public Interface {
public:
    NWM_INF();

    std::string GetPortName() const override {
        return "nwm::INF";
    }
};

} // namespace NWM
} // namespace Service
