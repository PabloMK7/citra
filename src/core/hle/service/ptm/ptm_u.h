// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace PTM {

class PTM_U final : public Interface {
public:
    PTM_U();

    std::string GetPortName() const override {
        return "ptm:u";
    }
};

} // namespace PTM
} // namespace Service