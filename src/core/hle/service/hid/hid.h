// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>

#include "core/hle/kernel/kernel.h"
#include "core/hle/service/service.h"
#include "common/bit_field.h"

namespace Kernel {
    class SharedMemory;
    class Event;
}

namespace Service {
namespace HID {

// Handle to shared memory region designated to HID_User service
extern Kernel::SharedPtr<Kernel::SharedMemory> g_shared_mem;

// Event handles
extern Kernel::SharedPtr<Kernel::Event> g_event_pad_or_touch_1;
extern Kernel::SharedPtr<Kernel::Event> g_event_pad_or_touch_2;
extern Kernel::SharedPtr<Kernel::Event> g_event_accelerometer;
extern Kernel::SharedPtr<Kernel::Event> g_event_gyroscope;
extern Kernel::SharedPtr<Kernel::Event> g_event_debug_pad;

/**
 * Structure of a Pad controller state.
 */
struct PadState {
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

        BitField<14, 1, u32> zl;
        BitField<15, 1, u32> zr;

        BitField<20, 1, u32> touch;

        BitField<24, 1, u32> c_right;
        BitField<25, 1, u32> c_left;
        BitField<26, 1, u32> c_up;
        BitField<27, 1, u32> c_down;
        BitField<28, 1, u32> circle_right;
        BitField<29, 1, u32> circle_left;
        BitField<30, 1, u32> circle_up;
        BitField<31, 1, u32> circle_down;
    };
};

/**
 * Structure of a single entry of Pad state history within HID shared memory
 */
struct PadDataEntry {
    PadState current_state;
    PadState delta_additions;
    PadState delta_removals;

    s16 circle_pad_x;
    s16 circle_pad_y;
};

/**
 * Structure of a single entry of touch state history within HID shared memory
 */
struct TouchDataEntry {
    u16 x;
    u16 y;
    u32 data_valid;
};

/**
 * Structure of data stored in HID shared memory
 */
struct SharedMem {
    // Offset 0x0 : "PAD" data, this is used for buttons and the circle pad
    struct {
        s64 index_reset_ticks;
        s64 index_reset_ticks_previous;
        u32 index; // Index of the last updated pad state history element

        INSERT_PADDING_BYTES(0x8);

        PadState current_state; // Same as entries[index].current_state
        u32 raw_circle_pad_data;

        INSERT_PADDING_BYTES(0x4);

        std::array<PadDataEntry, 8> entries; // Pad state history
    } pad;

    // Offset 0xA8 : Touchpad data, this is used for touchpad input
    struct {
        s64 index_reset_ticks;
        s64 index_reset_ticks_previous;
        u32 index; // Index of the last updated touch state history element

        INSERT_PADDING_BYTES(0xC);

        std::array<TouchDataEntry, 8> entries;
    } touch;
};

// Pre-defined PadStates for single button presses
const PadState PAD_NONE         = {{0}};
const PadState PAD_A            = {{1u << 0}};
const PadState PAD_B            = {{1u << 1}};
const PadState PAD_SELECT       = {{1u << 2}};
const PadState PAD_START        = {{1u << 3}};
const PadState PAD_RIGHT        = {{1u << 4}};
const PadState PAD_LEFT         = {{1u << 5}};
const PadState PAD_UP           = {{1u << 6}};
const PadState PAD_DOWN         = {{1u << 7}};
const PadState PAD_R            = {{1u << 8}};
const PadState PAD_L            = {{1u << 9}};
const PadState PAD_X            = {{1u << 10}};
const PadState PAD_Y            = {{1u << 11}};

const PadState PAD_ZL           = {{1u << 14}};
const PadState PAD_ZR           = {{1u << 15}};

const PadState PAD_TOUCH        = {{1u << 20}};

const PadState PAD_C_RIGHT      = {{1u << 24}};
const PadState PAD_C_LEFT       = {{1u << 25}};
const PadState PAD_C_UP         = {{1u << 26}};
const PadState PAD_C_DOWN       = {{1u << 27}};
const PadState PAD_CIRCLE_RIGHT = {{1u << 28}};
const PadState PAD_CIRCLE_LEFT  = {{1u << 29}};
const PadState PAD_CIRCLE_UP    = {{1u << 30}};
const PadState PAD_CIRCLE_DOWN  = {{1u << 31}};

/**
 * HID::GetIPCHandles service function
 *  Inputs:
 *      None
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : Unused
 *      3 : Handle to HID_User shared memory
 *      4 : Event signaled by HID_User
 *      5 : Event signaled by HID_User
 *      6 : Event signaled by HID_User
 *      7 : Gyroscope event
 *      8 : Event signaled by HID_User
 */
void GetIPCHandles(Interface* self);

/**
 * Sets a Pad state (button or button combo) as pressed
 * @param pad_state PadState data indicating which buttons have been pressed
 */
void PadButtonPress(const PadState& pad_state);

/**
 * Sets a Pad state (button or button combo) as released
 * @param pad_state PadState data indicating which buttons have been released
 */
void PadButtonRelease(const PadState& pad_state);

/**
 * Called after all Pad changes to be included in this update have been made, including both Pad
 * key changes and analog circle Pad changes.
 */
void PadUpdateComplete();

void HIDInit();
void HIDShutdown();

}
}
