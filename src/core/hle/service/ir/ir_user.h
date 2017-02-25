// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace IR {

class IR_User_Interface : public Service::Interface {
public:
    IR_User_Interface();

    std::string GetPortName() const override {
        return "ir:USER";
    }
};

void InitUser();
void ShutdownUser();

} // namespace IR
} // namespace Service
