// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace CFG {

class CFG_I final : public Interface {
public:
    CFG_I();

    std::string GetPortName() const override {
        return "cfg:i";
    }
};

} // namespace CFG
} // namespace Service