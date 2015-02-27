// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

namespace Service {
namespace HID {

class HID_SPVR_Interface : public Service::Interface {
public:
    HID_SPVR_Interface();

    std::string GetPortName() const override {
        return "hid:SPVR";
    }
};

} // namespace HID
} // namespace Service