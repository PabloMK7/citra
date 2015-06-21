// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace CAM {

class CAM_U_Interface : public Service::Interface {
public:
    CAM_U_Interface();

    std::string GetPortName() const override {
        return "cam:u";
    }
};

} // namespace CAM
} // namespace Service
