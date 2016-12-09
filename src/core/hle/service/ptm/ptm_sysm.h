// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace PTM {

class PTM_S final : public Interface {
public:
    PTM_S();

    std::string GetPortName() const override {
        return "ptm:s";
    }
};

class PTM_Sysm final : public Interface {
public:
    PTM_Sysm();

    std::string GetPortName() const override {
        return "ptm:sysm";
    }
};

} // namespace PTM
} // namespace Service
