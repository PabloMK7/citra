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

namespace Core {
struct CTMHeader;
struct ControllerState;
enum class PlayMode;

class Movie {
public:
    /**
    * Gets the instance of the Movie singleton class.
    * @returns Reference to the instance of the Movie singleton class.
    */
    static Movie& GetInstance() {
        return s_instance;
    }

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

private:
    static Movie s_instance;

    bool IsPlayingInput();

    bool IsRecordingInput();

    void CheckInputEnd();

    template <typename... Targs>
    void Handle(Targs&... Fargs);

    void Play(Service::HID::PadState& pad_state, s16& circle_pad_x, s16& circle_pad_y);
    void Play(Service::HID::TouchDataEntry& touch_data);
    void Play(Service::HID::AccelerometerDataEntry& accelerometer_data);
    void Play(Service::HID::GyroscopeDataEntry& gyroscope_data);
    void Play(Service::IR::PadState& pad_state, s16& c_stick_x, s16& c_stick_y);
    void Play(Service::IR::ExtraHIDResponse& extra_hid_response);

    void Record(const ControllerState& controller_state);
    void Record(const Service::HID::PadState& pad_state, const s16& circle_pad_x,
                const s16& circle_pad_y);
    void Record(const Service::HID::TouchDataEntry& touch_data);
    void Record(const Service::HID::AccelerometerDataEntry& accelerometer_data);
    void Record(const Service::HID::GyroscopeDataEntry& gyroscope_data);
    void Record(const Service::IR::PadState& pad_state, const s16& c_stick_x, const s16& c_stick_y);
    void Record(const Service::IR::ExtraHIDResponse& extra_hid_response);

    bool ValidateHeader(const CTMHeader& header);

    void SaveMovie();

    PlayMode play_mode;
    std::vector<u8> recorded_input;
    size_t current_byte = 0;
};
} // namespace Core