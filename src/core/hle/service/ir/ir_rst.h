// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included..

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace IR {

class IR_RST_Interface : public Service::Interface {
public:
    IR_RST_Interface();

    std::string GetPortName() const override {
        return "ir:rst";
    }
};

void InitRST();
void ShutdownRST();

/// Reload input devices. Used when input configuration changed
void ReloadInputDevicesRST();

} // namespace IR
} // namespace Service
