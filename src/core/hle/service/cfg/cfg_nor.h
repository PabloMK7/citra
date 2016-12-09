// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace CFG {

class CFG_NOR final : public Interface {
public:
    CFG_NOR();

    std::string GetPortName() const override {
        return "cfg:nor";
    }
};

} // namespace CFG
} // namespace Service
