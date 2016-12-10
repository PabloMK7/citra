// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace PM {

class PM_APP final : public Interface {
public:
    PM_APP();

    std::string GetPortName() const override {
        return "pm:app";
    }
};

} // namespace PM
} // namespace Service
