// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace NIM {

class NIM_U_Interface : public Service::Interface {
public:
    NIM_U_Interface();

    std::string GetPortName() const override {
        return "nim:u";
    }
};

} // namespace NIM
} // namespace Service
