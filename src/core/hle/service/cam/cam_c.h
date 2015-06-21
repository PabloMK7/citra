// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace CAM {

class CAM_C_Interface : public Service::Interface {
public:
    CAM_C_Interface();

    std::string GetPortName() const override {
        return "cam:c";
    }
};

} // namespace CAM
} // namespace Service
