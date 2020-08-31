// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <boost/serialization/vector.hpp>
#include "common/common_types.h"

namespace Service {
namespace HID {
struct AccelerometerDataEntry;
struct GyroscopeDataEntry;
struct PadState;
struct TouchDataEntry;
} // namespace HID
namespace IR {
struct ExtraHIDResponse;
union PadState;
} // namespace IR
} // namespace Service

namespace Core {
struct CTMHeader;
struct ControllerState;
enum class PlayMode;

class Movie {
public:
    enum class ValidationResult {
        OK,
        RevisionDismatch,
        GameDismatch,
        Invalid,
    };
    /**
     * Gets the instance of the Movie singleton class.
     * @returns Reference to the instance of the Movie singleton class.
     */
    static Movie& GetInstance() {
        return s_instance;
    }

    void StartPlayback(
        const std::string& movie_file, std::function<void()> completion_callback = [] {});
    void StartRecording(const std::string& movie_file);

    /// Prepare to override the clock before playing back movies
    void PrepareForPlayback(const std::string& movie_file);

    /// Prepare to override the clock before recording movies
    void PrepareForRecording();

    ValidationResult ValidateMovie(const std::string& movie_file, u64 program_id = 0) const;

    /// Get the init time that would override the one in the settings
    u64 GetOverrideInitTime() const;
    u64 GetMovieProgramID(const std::string& movie_file) const;

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
    bool IsPlayingInput() const;
    bool IsRecordingInput() const;

private:
    static Movie s_instance;

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

    ValidationResult ValidateHeader(const CTMHeader& header, u64 program_id = 0) const;

    void SaveMovie();

    PlayMode play_mode;
    std::string record_movie_file;
    std::vector<u8> recorded_input;
    u64 init_time;
    std::function<void()> playback_completion_callback;
    std::size_t current_byte = 0;

    template <class Archive>
    void serialize(Archive& ar, const unsigned int) {
        // Only serialize what's needed to make savestates useful for TAS:
        u64 _current_byte = static_cast<u64>(current_byte);
        ar& _current_byte;
        current_byte = static_cast<std::size_t>(_current_byte);
        ar& recorded_input;
        ar& init_time;
    }
    friend class boost::serialization::access;
};
} // namespace Core