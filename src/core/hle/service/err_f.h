// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace ERR {

class ERR_F final : public Interface {
public:
    ERR_F();

    std::string GetPortName() const override {
        return "err:f";
    }
};

} // namespace ERR
} // namespace Service
