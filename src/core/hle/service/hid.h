// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include "core/hle/service/service.h"
#include "common/bit_field.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace HID_User

// This service is used for interfacing to physical user controls... perhaps "Human Interface 
// Devices"? Uses include game pad controls, accelerometers, gyroscopes, etc.

namespace HID_User {

struct PADState {
    union {
        u32 hex;

        BitField<0, 1, u32> A;
        BitField<1, 1, u32> B;
        BitField<2, 1, u32> Select;
        BitField<3, 1, u32> Start;
        BitField<4, 1, u32> Right;
        BitField<5, 1, u32> Left;
        BitField<6, 1, u32> Up;
        BitField<7, 1, u32> Down;
        BitField<8, 1, u32> R;
        BitField<9, 1, u32> L;
        BitField<10, 1, u32> X;
        BitField<11, 1, u32> Y;

        BitField<28, 1, u32> CircleRight;
        BitField<29, 1, u32> CircleLeft;
        BitField<30, 1, u32> CircleUp;
        BitField<31, 1, u32> CircleDown;
    };
};

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
