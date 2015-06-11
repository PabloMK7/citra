// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace CECD {

class CECD_U_Interface : public Interface {
public:
    CECD_U_Interface();

    std::string GetPortName() const override {
        return "cecd:u";
    }
};

} // namespace CECD
} // namespace Service
