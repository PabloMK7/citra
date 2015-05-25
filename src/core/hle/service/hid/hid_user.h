// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

// This service is used for interfacing to physical user controls.
// Uses include game pad controls, touchscreen, accelerometers, gyroscopes, and debug pad.

namespace Service {
namespace HID {

/**
 * HID service interface.
 */
class HID_U_Interface : public Service::Interface {
public:
    HID_U_Interface();

    std::string GetPortName() const override {
        return "hid:USER";
    }
};

} // namespace HID
} // namespace Service