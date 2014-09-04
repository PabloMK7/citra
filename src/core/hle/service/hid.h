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

/// Structure of a PAD controller state
struct PADState {
    union {
        u32 hex;

        BitField<0, 1, u32> a;
        BitField<1, 1, u32> b;
        BitField<2, 1, u32> select;
        BitField<3, 1, u32> start;
        BitField<4, 1, u32> right;
        BitField<5, 1, u32> left;
        BitField<6, 1, u32> up;
        BitField<7, 1, u32> down;
        BitField<8, 1, u32> r;
        BitField<9, 1, u32> l;
        BitField<10, 1, u32> x;
        BitField<11, 1, u32> y;

        BitField<28, 1, u32> circle_right;
        BitField<29, 1, u32> circle_left;
        BitField<30, 1, u32> circle_up;
        BitField<31, 1, u32> circle_down;
    };
};

/// Structure of a single entry in the PADData's PAD state history array
struct PADDataEntry {
    PADState current_state;
    PADState delta_additions;
    PADState delta_removals;

    s16 circle_pad_x;
    s16 circle_pad_y;
};

/// Structure of all data related to the 3DS Pad
struct PADData {
    s64 index_reset_ticks;
    s64 index_reset_ticks_previous;
    u32 index; // the index of the last updated PAD state history element

    u32 pad1;
    u32 pad2;

    PADState current_state; // same as entries[index].current_state
    u32 raw_circle_pad_data;

    u32 pad3;

    std::array<PADDataEntry, 8> entries; // PAD state history
};

// Pre-defined PADStates for single button presses
const PADState PAD_NONE         = {{0}};
const PADState PAD_A            = {{1u << 0}};
const PADState PAD_B            = {{1u << 1}};
const PADState PAD_SELECT       = {{1u << 2}};
const PADState PAD_START        = {{1u << 3}};
const PADState PAD_RIGHT        = {{1u << 4}};
const PADState PAD_LEFT         = {{1u << 5}};
const PADState PAD_UP           = {{1u << 6}};
const PADState PAD_DOWN         = {{1u << 7}};
const PADState PAD_R            = {{1u << 8}};
const PADState PAD_L            = {{1u << 9}};
const PADState PAD_X            = {{1u << 10}};
const PADState PAD_Y            = {{1u << 11}};
const PADState PAD_CIRCLE_RIGHT = {{1u << 28}};
const PADState PAD_CIRCLE_LEFT  = {{1u << 29}};
const PADState PAD_CIRCLE_UP    = {{1u << 30}};
const PADState PAD_CIRCLE_DOWN  = {{1u << 31}};

// Methods for updating the HID module's state
void PADButtonPress(PADState pad_state);
void PADButtonRelease(PADState pad_state);
void PADUpdateComplete();

/// HID service interface
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
