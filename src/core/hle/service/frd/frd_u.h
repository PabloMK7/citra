// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace FRD {

class FRD_U_Interface : public Service::Interface {
public:
    FRD_U_Interface();

    std::string GetPortName() const override {
        return "frd:u";
    }
};

} // namespace FRD
} // namespace Service
