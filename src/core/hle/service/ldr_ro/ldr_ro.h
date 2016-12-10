// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace LDR {

class LDR_RO final : public Interface {
public:
    LDR_RO();

    std::string GetPortName() const override {
        return "ldr:ro";
    }
};

} // namespace LDR
} // namespace Service
