// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace CECD {

class CECD_S_Interface : public Interface {
public:
    CECD_S_Interface();

    std::string GetPortName() const override {
        return "cecd:s";
    }
};

} // namespace CECD
} // namespace Service
