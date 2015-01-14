// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace HID_SPVR

// This service is used for interfacing to physical user controls.
// Uses include game pad controls, touchscreen, accelerometers, gyroscopes, and debug pad.

namespace HID_SPVR {

/**
 * HID service interface.
 */
class Interface : public Service::Interface {
public:
    Interface();

    std::string GetPortName() const override {
        return "hid:SPVR";
    }
};

} // namespace
