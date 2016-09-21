// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#ifndef _MSC_VER
#include <cstddef>
#endif
#include "common/bit_field.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "core/settings.h"

namespace Service {

class Interface;

namespace HID {

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
    u16 x;                     ///< Y-coordinate of a touchpad press on the lower screen
    u16 y;                     ///< X-coordinate of a touchpad press on the lower screen
    BitField<0, 7, u32> valid; ///< Set to 1 when this entry contains actual X/Y data, otherwise 0
};

/**
 * Structure of a single entry of accelerometer state history within HID shared memory
 */
struct AccelerometerDataEntry {
    s16 x;
    s16 y;
    s16 z;
};

/**
 * Structure of a single entry of gyroscope state history within HID shared memory
 */
struct GyroscopeDataEntry {
    s16 x;
    s16 y;
    s16 z;
};

/**
 * Structure of data stored in HID shared memory
 */
struct SharedMem {
    /// Pad data, this is used for buttons and the circle pad
    struct {
        s64 index_reset_ticks; ///< CPU tick count for when HID module updated entry index 0
        s64 index_reset_ticks_previous; ///< Previous `index_reset_ticks`
        u32 index;                      ///< Index of the last updated pad state entry

        INSERT_PADDING_WORDS(0x2);

        PadState current_state; ///< Current state of the pad buttons

        // TODO(bunnei): Implement `raw_circle_pad_data` field
        u32 raw_circle_pad_data; ///< Raw (analog) circle pad data, before being converted

        INSERT_PADDING_WORDS(0x1);

        std::array<PadDataEntry, 8> entries; ///< Last 8 pad entries
    } pad;

    /// Touchpad data, this is used for touchpad input
    struct {
        s64 index_reset_ticks; ///< CPU tick count for when HID module updated entry index 0
        s64 index_reset_ticks_previous; ///< Previous `index_reset_ticks`
        u32 index;                      ///< Index of the last updated touch entry

        INSERT_PADDING_WORDS(0x1);

        // TODO(bunnei): Implement `raw_entry` field
        TouchDataEntry raw_entry; ///< Raw (analog) touch data, before being converted

        std::array<TouchDataEntry, 8> entries; ///< Last 8 touch entries, in pixel coordinates
    } touch;

    /// Accelerometer data
    struct {
        s64 index_reset_ticks; ///< CPU tick count for when HID module updated entry index 0
        s64 index_reset_ticks_previous; ///< Previous `index_reset_ticks`
        u32 index;                      ///< Index of the last updated accelerometer entry

        INSERT_PADDING_WORDS(0x1);

        AccelerometerDataEntry raw_entry;
        INSERT_PADDING_BYTES(2);

        std::array<AccelerometerDataEntry, 8> entries;
    } accelerometer;

    /// Gyroscope data
    struct {
        s64 index_reset_ticks; ///< CPU tick count for when HID module updated entry index 0
        s64 index_reset_ticks_previous; ///< Previous `index_reset_ticks`
        u32 index;                      ///< Index of the last updated accelerometer entry

        INSERT_PADDING_WORDS(0x1);

        GyroscopeDataEntry raw_entry;
        INSERT_PADDING_BYTES(2);

        std::array<GyroscopeDataEntry, 32> entries;
    } gyroscope;
};

/**
 * Structure of calibrate params that GetGyroscopeLowCalibrateParam returns
 */
struct GyroscopeCalibrateParam {
    struct {
        // TODO (wwylele): figure out the exact meaning of these params
        s16 zero_point;
        s16 positive_unit_point;
        s16 negative_unit_point;
    } x, y, z;
};

// TODO: MSVC does not support using offsetof() on non-static data members even though this
//       is technically allowed since C++11. This macro should be enabled once MSVC adds
//       support for that.
#ifndef _MSC_VER
#define ASSERT_REG_POSITION(field_name, position)                                                  \
    static_assert(offsetof(SharedMem, field_name) == position * 4,                                 \
                  "Field " #field_name " has invalid position")

ASSERT_REG_POSITION(pad.index_reset_ticks, 0x0);
ASSERT_REG_POSITION(touch.index_reset_ticks, 0x2A);

#undef ASSERT_REG_POSITION
#endif // !defined(_MSC_VER)

// Pre-defined PadStates for single button presses
const PadState PAD_NONE = {{0}};
const PadState PAD_A = {{1u << 0}};
const PadState PAD_B = {{1u << 1}};
const PadState PAD_SELECT = {{1u << 2}};
const PadState PAD_START = {{1u << 3}};
const PadState PAD_RIGHT = {{1u << 4}};
const PadState PAD_LEFT = {{1u << 5}};
const PadState PAD_UP = {{1u << 6}};
const PadState PAD_DOWN = {{1u << 7}};
const PadState PAD_R = {{1u << 8}};
const PadState PAD_L = {{1u << 9}};
const PadState PAD_X = {{1u << 10}};
const PadState PAD_Y = {{1u << 11}};

const PadState PAD_ZL = {{1u << 14}};
const PadState PAD_ZR = {{1u << 15}};

const PadState PAD_TOUCH = {{1u << 20}};

const PadState PAD_C_RIGHT = {{1u << 24}};
const PadState PAD_C_LEFT = {{1u << 25}};
const PadState PAD_C_UP = {{1u << 26}};
const PadState PAD_C_DOWN = {{1u << 27}};
const PadState PAD_CIRCLE_RIGHT = {{1u << 28}};
const PadState PAD_CIRCLE_LEFT = {{1u << 29}};
const PadState PAD_CIRCLE_UP = {{1u << 30}};
const PadState PAD_CIRCLE_DOWN = {{1u << 31}};

/**
 * HID::GetIPCHandles service function
 *  Inputs:
 *      None
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : IPC Command Structure translate-header
 *      3 : Handle to HID shared memory
 *      4 : Event signaled by HID
 *      5 : Event signaled by HID
 *      6 : Event signaled by HID
 *      7 : Gyroscope event
 *      8 : Event signaled by HID
 */
void GetIPCHandles(Interface* self);

/**
 * HID::EnableAccelerometer service function
 *  Inputs:
 *      None
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void EnableAccelerometer(Interface* self);

/**
 * HID::DisableAccelerometer service function
 *  Inputs:
 *      None
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void DisableAccelerometer(Interface* self);

/**
 * HID::EnableGyroscopeLow service function
 *  Inputs:
 *      None
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void EnableGyroscopeLow(Interface* self);

/**
 * HID::DisableGyroscopeLow service function
 *  Inputs:
 *      None
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
void DisableGyroscopeLow(Interface* self);

/**
 * HID::GetSoundVolume service function
 *  Inputs:
 *      None
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8 output value
 */
void GetSoundVolume(Interface* self);

/**
 * HID::GetGyroscopeLowRawToDpsCoefficient service function
 *  Inputs:
 *      None
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : float output value
 */
void GetGyroscopeLowRawToDpsCoefficient(Service::Interface* self);

/**
 * HID::GetGyroscopeLowCalibrateParam service function
 *  Inputs:
 *      None
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2~6 (18 bytes) : struct GyroscopeCalibrateParam
 */
void GetGyroscopeLowCalibrateParam(Service::Interface* self);

/// Checks for user input updates
void Update();

/// Initialize HID service
void Init();

/// Shutdown HID service
void Shutdown();
}
}
