// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace PTM {

class PTM_Sets final : public Interface {
public:
    PTM_Sets();

    std::string GetPortName() const override {
        return "ptm:sets";
    }
};

} // namespace PTM
} // namespace Service
