// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace PTM {

class PTM_Gets final : public Interface {
public:
    PTM_Gets();

    std::string GetPortName() const override {
        return "ptm:gets";
    }
};

} // namespace PTM
} // namespace Service
