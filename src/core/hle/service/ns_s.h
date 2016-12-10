// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace NS {

class NS_S final : public Interface {
public:
    NS_S();

    std::string GetPortName() const override {
        return "ns:s";
    }
};

} // namespace NS
} // namespace Service
