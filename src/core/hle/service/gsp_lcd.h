// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace GSP {

class GSP_LCD final : public Interface {
public:
    GSP_LCD();

    std::string GetPortName() const override {
        return "gsp::Lcd";
    }
};

} // namespace GSP
} // namespace Service
