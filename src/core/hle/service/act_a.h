// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace ACT {

class ACT_A final : public Service::Interface {
public:
    ACT_A();

    std::string GetPortName() const override {
        return "act:a";
    }
};

} // namespace ACT
} // namespace Service
