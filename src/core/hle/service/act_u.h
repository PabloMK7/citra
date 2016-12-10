// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace ACT {

class ACT_U final : public Interface {
public:
    ACT_U();

    std::string GetPortName() const override {
        return "act:u";
    }
};

} // namespace ACT
} // namespace Service
