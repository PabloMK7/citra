// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace HID_User

// This service is used for interfacing to physical user controls... perhaps "Human Interface 
// Devices"? Uses include game pad controls, accelerometers, gyroscopes, etc.

namespace HID_User {

class Interface : public Service::Interface {
public:

    Interface();

    ~Interface();

    /**
     * Gets the string port name used by CTROS for the service
     * @return Port name of service
     */
    std::string GetPortName() const {
        return "hid:USER";
    }

};

} // namespace
