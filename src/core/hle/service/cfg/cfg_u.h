// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace CFG {

class CFG_U final : public Interface {
public:
    CFG_U();

    std::string GetPortName() const override {
        return "cfg:u";
    }
};

} // namespace CFG
} // namespace Service
