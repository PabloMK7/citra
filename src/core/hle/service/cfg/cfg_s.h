// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace CFG {

class CFG_S final : public Interface {
public:
    CFG_S();

    std::string GetPortName() const override {
        return "cfg:s";
    }
};

} // namespace CFG
} // namespace Service
