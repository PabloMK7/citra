// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace FRD {

class FRD_A_Interface : public Service::Interface {
public:
    FRD_A_Interface();

    std::string GetPortName() const override {
        return "frd:a";
    }
};

} // namespace FRD
} // namespace Service
