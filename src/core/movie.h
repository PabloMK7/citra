// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"

namespace Service {
namespace HID {
struct AccelerometerDataEntry;
struct GyroscopeDataEntry;
struct PadState;
struct TouchDataEntry;
}
namespace IR {
struct ExtraHIDResponse;
union PadState;
}
}

namespace Movie {

void Init();

void Shutdown();

/**
 * When recording: Takes a copy of the given input states so they can be used for playback
 * When playing: Replaces the given input states with the ones stored in the playback file
 */
void HandlePadAndCircleStatus(Service::HID::PadState& pad_state, s16& circle_pad_x,
                              s16& circle_pad_y);

/**
* When recording: Takes a copy of the given input states so they can be used for playback
* When playing: Replaces the given input states with the ones stored in the playback file
*/
void HandleTouchStatus(Service::HID::TouchDataEntry& touch_data);

/**
* When recording: Takes a copy of the given input states so they can be used for playback
* When playing: Replaces the given input states with the ones stored in the playback file
*/
void HandleAccelerometerStatus(Service::HID::AccelerometerDataEntry& accelerometer_data);

/**
* When recording: Takes a copy of the given input states so they can be used for playback
* When playing: Replaces the given input states with the ones stored in the playback file
*/
void HandleGyroscopeStatus(Service::HID::GyroscopeDataEntry& gyroscope_data);

/**
* When recording: Takes a copy of the given input states so they can be used for playback
* When playing: Replaces the given input states with the ones stored in the playback file
*/
void HandleIrRst(Service::IR::PadState& pad_state, s16& c_stick_x, s16& c_stick_y);

/**
* When recording: Takes a copy of the given input states so they can be used for playback
* When playing: Replaces the given input states with the ones stored in the playback file
*/
void HandleExtraHidResponse(Service::IR::ExtraHIDResponse& extra_hid_response);
}
