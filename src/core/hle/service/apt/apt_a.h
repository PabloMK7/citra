// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace APT {

class APT_A_Interface : public Service::Interface {
public:
    APT_A_Interface();

    std::string GetPortName() const override {
        return "APT:A";
    }
};

} // namespace APT
} // namespace Service