// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

// Local-WLAN service

namespace Service {
namespace NWM {

class NWM_UDS final : public Interface {
public:
    NWM_UDS();
    ~NWM_UDS() override;

    std::string GetPortName() const override {
        return "nwm::UDS";
    }
};

} // namespace NWM
} // namespace Service
