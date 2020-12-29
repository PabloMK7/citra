// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include <string>
#include <vector>
#include <boost/optional.hpp>
#include <cryptopp/hex.h>
#include "common/bit_field.h"
#include "common/common_types.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/scm_rev.h"
#include "common/string_util.h"
#include "common/swap.h"
#include "common/timer.h"
#include "core/core.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/extra_hid.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/movie.h"

namespace Core {

/*static*/ Movie Movie::s_instance;

enum class PlayMode { None, Recording, Playing };

enum class ControllerStateType : u8 {
    PadAndCircle,
    Touch,
    Accelerometer,
    Gyroscope,
    IrRst,
    ExtraHidResponse
};

#pragma pack(push, 1)
struct ControllerState {
    ControllerStateType type;

    union {
        struct {
            union {
                u16_le hex;

                BitField<0, 1, u16> a;
                BitField<1, 1, u16> b;
                BitField<2, 1, u16> select;
                BitField<3, 1, u16> start;
                BitField<4, 1, u16> right;
                BitField<5, 1, u16> left;
                BitField<6, 1, u16> up;
                BitField<7, 1, u16> down;
                BitField<8, 1, u16> r;
                BitField<9, 1, u16> l;
                BitField<10, 1, u16> x;
                BitField<11, 1, u16> y;
                BitField<12, 1, u16> debug;
                BitField<13, 1, u16> gpio14;
                // Bits 14-15 are currently unused
            };
            s16_le circle_pad_x;
            s16_le circle_pad_y;
        } pad_and_circle;

        struct {
            u16_le x;
            u16_le y;
            // This is a bool, u8 for platform compatibility
            u8 valid;
        } touch;

        struct {
            s16_le x;
            s16_le y;
            s16_le z;
        } accelerometer;

        struct {
            s16_le x;
            s16_le y;
            s16_le z;
        } gyroscope;

        struct {
            s16_le x;
            s16_le y;
            // These are bool, u8 for platform compatibility
            u8 zl;
            u8 zr;
        } ir_rst;

        struct {
            union {
                u32_le hex;

                BitField<0, 5, u32> battery_level;
                BitField<5, 1, u32> zl_not_held;
                BitField<6, 1, u32> zr_not_held;
                BitField<7, 1, u32> r_not_held;
                BitField<8, 12, u32> c_stick_x;
                BitField<20, 12, u32> c_stick_y;
            };
        } extra_hid_response;
    };
};
static_assert(sizeof(ControllerState) == 7, "ControllerState should be 7 bytes");
#pragma pack(pop)

constexpr std::array<u8, 4> header_magic_bytes{{'C', 'T', 'M', 0x1B}};

#pragma pack(push, 1)
struct CTMHeader {
    std::array<u8, 4> filetype;  /// Unique Identifier to check the file type (always "CTM"0x1B)
    u64_le program_id;           /// ID of the ROM being executed. Also called title_id
    std::array<u8, 20> revision; /// Git hash of the revision this movie was created with
    u64_le clock_init_time;      /// The init time of the system clock

    std::array<u8, 216> reserved; /// Make heading 256 bytes so it has consistent size
};
static_assert(sizeof(CTMHeader) == 256, "CTMHeader should be 256 bytes");
#pragma pack(pop)

bool Movie::IsPlayingInput() const {
    return play_mode == PlayMode::Playing;
}
bool Movie::IsRecordingInput() const {
    return play_mode == PlayMode::Recording;
}

void Movie::CheckInputEnd() {
    if (current_byte + sizeof(ControllerState) > recorded_input.size()) {
        LOG_INFO(Movie, "Playback finished");
        play_mode = PlayMode::None;
        init_time = 0;
        playback_completion_callback();
    }
}

void Movie::Play(Service::HID::PadState& pad_state, s16& circle_pad_x, s16& circle_pad_y) {
    ControllerState s;
    std::memcpy(&s, &recorded_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::PadAndCircle) {
        LOG_ERROR(Movie,
                  "Expected to read type {}, but found {}. Your playback will be out of sync",
                  static_cast<int>(ControllerStateType::PadAndCircle), s.type);
        return;
    }

    pad_state.a.Assign(s.pad_and_circle.a);
    pad_state.b.Assign(s.pad_and_circle.b);
    pad_state.select.Assign(s.pad_and_circle.select);
    pad_state.start.Assign(s.pad_and_circle.start);
    pad_state.right.Assign(s.pad_and_circle.right);
    pad_state.left.Assign(s.pad_and_circle.left);
    pad_state.up.Assign(s.pad_and_circle.up);
    pad_state.down.Assign(s.pad_and_circle.down);
    pad_state.r.Assign(s.pad_and_circle.r);
    pad_state.l.Assign(s.pad_and_circle.l);
    pad_state.x.Assign(s.pad_and_circle.x);
    pad_state.y.Assign(s.pad_and_circle.y);
    pad_state.debug.Assign(s.pad_and_circle.debug);
    pad_state.gpio14.Assign(s.pad_and_circle.gpio14);

    circle_pad_x = s.pad_and_circle.circle_pad_x;
    circle_pad_y = s.pad_and_circle.circle_pad_y;
}

void Movie::Play(Service::HID::TouchDataEntry& touch_data) {
    ControllerState s;
    std::memcpy(&s, &recorded_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::Touch) {
        LOG_ERROR(Movie,
                  "Expected to read type {}, but found {}. Your playback will be out of sync",
                  static_cast<int>(ControllerStateType::Touch), s.type);
        return;
    }

    touch_data.x = s.touch.x;
    touch_data.y = s.touch.y;
    touch_data.valid.Assign(s.touch.valid);
}

void Movie::Play(Service::HID::AccelerometerDataEntry& accelerometer_data) {
    ControllerState s;
    std::memcpy(&s, &recorded_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::Accelerometer) {
        LOG_ERROR(Movie,
                  "Expected to read type {}, but found {}. Your playback will be out of sync",
                  static_cast<int>(ControllerStateType::Accelerometer), s.type);
        return;
    }

    accelerometer_data.x = s.accelerometer.x;
    accelerometer_data.y = s.accelerometer.y;
    accelerometer_data.z = s.accelerometer.z;
}

void Movie::Play(Service::HID::GyroscopeDataEntry& gyroscope_data) {
    ControllerState s;
    std::memcpy(&s, &recorded_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::Gyroscope) {
        LOG_ERROR(Movie,
                  "Expected to read type {}, but found {}. Your playback will be out of sync",
                  static_cast<int>(ControllerStateType::Gyroscope), s.type);
        return;
    }

    gyroscope_data.x = s.gyroscope.x;
    gyroscope_data.y = s.gyroscope.y;
    gyroscope_data.z = s.gyroscope.z;
}

void Movie::Play(Service::IR::PadState& pad_state, s16& c_stick_x, s16& c_stick_y) {
    ControllerState s;
    std::memcpy(&s, &recorded_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::IrRst) {
        LOG_ERROR(Movie,
                  "Expected to read type {}, but found {}. Your playback will be out of sync",
                  static_cast<int>(ControllerStateType::IrRst), s.type);
        return;
    }

    c_stick_x = s.ir_rst.x;
    c_stick_y = s.ir_rst.y;
    pad_state.zl.Assign(s.ir_rst.zl);
    pad_state.zr.Assign(s.ir_rst.zr);
}

void Movie::Play(Service::IR::ExtraHIDResponse& extra_hid_response) {
    ControllerState s;
    std::memcpy(&s, &recorded_input[current_byte], sizeof(ControllerState));
    current_byte += sizeof(ControllerState);

    if (s.type != ControllerStateType::ExtraHidResponse) {
        LOG_ERROR(Movie,
                  "Expected to read type {}, but found {}. Your playback will be out of sync",
                  static_cast<int>(ControllerStateType::ExtraHidResponse), s.type);
        return;
    }

    extra_hid_response.buttons.battery_level.Assign(
        static_cast<u8>(s.extra_hid_response.battery_level));
    extra_hid_response.c_stick.c_stick_x.Assign(s.extra_hid_response.c_stick_x);
    extra_hid_response.c_stick.c_stick_y.Assign(s.extra_hid_response.c_stick_y);
    extra_hid_response.buttons.r_not_held.Assign(static_cast<u8>(s.extra_hid_response.r_not_held));
    extra_hid_response.buttons.zl_not_held.Assign(
        static_cast<u8>(s.extra_hid_response.zl_not_held));
    extra_hid_response.buttons.zr_not_held.Assign(
        static_cast<u8>(s.extra_hid_response.zr_not_held));
}

void Movie::Record(const ControllerState& controller_state) {
    recorded_input.resize(current_byte + sizeof(ControllerState));
    std::memcpy(&recorded_input[current_byte], &controller_state, sizeof(ControllerState));
    current_byte += sizeof(ControllerState);
}

void Movie::Record(const Service::HID::PadState& pad_state, const s16& circle_pad_x,
                   const s16& circle_pad_y) {
    ControllerState s;
    s.type = ControllerStateType::PadAndCircle;

    s.pad_and_circle.a.Assign(static_cast<u16>(pad_state.a));
    s.pad_and_circle.b.Assign(static_cast<u16>(pad_state.b));
    s.pad_and_circle.select.Assign(static_cast<u16>(pad_state.select));
    s.pad_and_circle.start.Assign(static_cast<u16>(pad_state.start));
    s.pad_and_circle.right.Assign(static_cast<u16>(pad_state.right));
    s.pad_and_circle.left.Assign(static_cast<u16>(pad_state.left));
    s.pad_and_circle.up.Assign(static_cast<u16>(pad_state.up));
    s.pad_and_circle.down.Assign(static_cast<u16>(pad_state.down));
    s.pad_and_circle.r.Assign(static_cast<u16>(pad_state.r));
    s.pad_and_circle.l.Assign(static_cast<u16>(pad_state.l));
    s.pad_and_circle.x.Assign(static_cast<u16>(pad_state.x));
    s.pad_and_circle.y.Assign(static_cast<u16>(pad_state.y));
    s.pad_and_circle.debug.Assign(static_cast<u16>(pad_state.debug));
    s.pad_and_circle.gpio14.Assign(static_cast<u16>(pad_state.gpio14));

    s.pad_and_circle.circle_pad_x = circle_pad_x;
    s.pad_and_circle.circle_pad_y = circle_pad_y;

    Record(s);
}

void Movie::Record(const Service::HID::TouchDataEntry& touch_data) {
    ControllerState s;
    s.type = ControllerStateType::Touch;

    s.touch.x = touch_data.x;
    s.touch.y = touch_data.y;
    s.touch.valid = static_cast<u8>(touch_data.valid);

    Record(s);
}

void Movie::Record(const Service::HID::AccelerometerDataEntry& accelerometer_data) {
    ControllerState s;
    s.type = ControllerStateType::Accelerometer;

    s.accelerometer.x = accelerometer_data.x;
    s.accelerometer.y = accelerometer_data.y;
    s.accelerometer.z = accelerometer_data.z;

    Record(s);
}

void Movie::Record(const Service::HID::GyroscopeDataEntry& gyroscope_data) {
    ControllerState s;
    s.type = ControllerStateType::Gyroscope;

    s.gyroscope.x = gyroscope_data.x;
    s.gyroscope.y = gyroscope_data.y;
    s.gyroscope.z = gyroscope_data.z;

    Record(s);
}

void Movie::Record(const Service::IR::PadState& pad_state, const s16& c_stick_x,
                   const s16& c_stick_y) {
    ControllerState s;
    s.type = ControllerStateType::IrRst;

    s.ir_rst.x = c_stick_x;
    s.ir_rst.y = c_stick_y;
    s.ir_rst.zl = static_cast<u8>(pad_state.zl);
    s.ir_rst.zr = static_cast<u8>(pad_state.zr);

    Record(s);
}

void Movie::Record(const Service::IR::ExtraHIDResponse& extra_hid_response) {
    ControllerState s;
    s.type = ControllerStateType::ExtraHidResponse;

    s.extra_hid_response.battery_level.Assign(extra_hid_response.buttons.battery_level);
    s.extra_hid_response.c_stick_x.Assign(extra_hid_response.c_stick.c_stick_x);
    s.extra_hid_response.c_stick_y.Assign(extra_hid_response.c_stick.c_stick_y);
    s.extra_hid_response.r_not_held.Assign(extra_hid_response.buttons.r_not_held);
    s.extra_hid_response.zl_not_held.Assign(extra_hid_response.buttons.zl_not_held);
    s.extra_hid_response.zr_not_held.Assign(extra_hid_response.buttons.zr_not_held);

    Record(s);
}

u64 Movie::GetOverrideInitTime() const {
    return init_time;
}

Movie::ValidationResult Movie::ValidateHeader(const CTMHeader& header, u64 program_id) const {
    if (header_magic_bytes != header.filetype) {
        LOG_ERROR(Movie, "Playback file does not have valid header");
        return ValidationResult::Invalid;
    }

    std::string revision = fmt::format("{:02x}", fmt::join(header.revision, ""));

    if (!program_id)
        Core::System::GetInstance().GetAppLoader().ReadProgramId(program_id);
    if (program_id != header.program_id) {
        LOG_WARNING(Movie, "This movie was recorded using a ROM with a different program id");
        return ValidationResult::GameDismatch;
    }

    if (revision != Common::g_scm_rev) {
        LOG_WARNING(Movie,
                    "This movie was created on a different version of Citra, playback may desync");
        return ValidationResult::RevisionDismatch;
    }

    return ValidationResult::OK;
}

void Movie::SaveMovie() {
    LOG_INFO(Movie, "Saving recorded movie to '{}'", record_movie_file);
    FileUtil::IOFile save_record(record_movie_file, "wb");

    if (!save_record.IsGood()) {
        LOG_ERROR(Movie, "Unable to open file to save movie");
        return;
    }

    CTMHeader header = {};
    header.filetype = header_magic_bytes;
    header.clock_init_time = init_time;

    Core::System::GetInstance().GetAppLoader().ReadProgramId(header.program_id);

    std::string rev_bytes;
    CryptoPP::StringSource(Common::g_scm_rev, true,
                           new CryptoPP::HexDecoder(new CryptoPP::StringSink(rev_bytes)));
    std::memcpy(header.revision.data(), rev_bytes.data(), sizeof(CTMHeader::revision));

    save_record.WriteBytes(&header, sizeof(CTMHeader));
    save_record.WriteBytes(recorded_input.data(), recorded_input.size());

    if (!save_record.IsGood()) {
        LOG_ERROR(Movie, "Error saving movie");
    }
}

void Movie::StartPlayback(const std::string& movie_file,
                          std::function<void()> completion_callback) {
    LOG_INFO(Movie, "Loading Movie for playback");
    FileUtil::IOFile save_record(movie_file, "rb");
    const u64 size = save_record.GetSize();

    if (save_record.IsGood() && size > sizeof(CTMHeader)) {
        CTMHeader header;
        save_record.ReadArray(&header, 1);
        if (ValidateHeader(header) != ValidationResult::Invalid) {
            play_mode = PlayMode::Playing;
            recorded_input.resize(size - sizeof(CTMHeader));
            save_record.ReadArray(recorded_input.data(), recorded_input.size());
            current_byte = 0;
            playback_completion_callback = completion_callback;
        }
    } else {
        LOG_ERROR(Movie, "Failed to playback movie: Unable to open '{}'", movie_file);
    }
}

void Movie::StartRecording(const std::string& movie_file) {
    LOG_INFO(Movie, "Enabling Movie recording");
    play_mode = PlayMode::Recording;
    record_movie_file = movie_file;
}

static boost::optional<CTMHeader> ReadHeader(const std::string& movie_file) {
    FileUtil::IOFile save_record(movie_file, "rb");
    const u64 size = save_record.GetSize();

    if (!save_record || size <= sizeof(CTMHeader)) {
        return boost::none;
    }

    CTMHeader header;
    save_record.ReadArray(&header, 1);

    if (header_magic_bytes != header.filetype) {
        return boost::none;
    }

    return header;
}

void Movie::PrepareForPlayback(const std::string& movie_file) {
    auto header = ReadHeader(movie_file);
    if (header == boost::none)
        return;

    init_time = header.value().clock_init_time;
}

void Movie::PrepareForRecording() {
    init_time = (Settings::values.init_clock == Settings::InitClock::SystemTime
                     ? Common::Timer::GetTimeSinceJan1970().count()
                     : Settings::values.init_time);
}

Movie::ValidationResult Movie::ValidateMovie(const std::string& movie_file, u64 program_id) const {
    LOG_INFO(Movie, "Validating Movie file '{}'", movie_file);
    auto header = ReadHeader(movie_file);
    if (header == boost::none)
        return ValidationResult::Invalid;

    return ValidateHeader(header.value(), program_id);
}

u64 Movie::GetMovieProgramID(const std::string& movie_file) const {
    auto header = ReadHeader(movie_file);
    if (header == boost::none)
        return 0;

    return static_cast<u64>(header.value().program_id);
}

void Movie::Shutdown() {
    if (IsRecordingInput()) {
        SaveMovie();
    }

    play_mode = PlayMode::None;
    recorded_input.resize(0);
    record_movie_file.clear();
    current_byte = 0;
    init_time = 0;
}

template <typename... Targs>
void Movie::Handle(Targs&... Fargs) {
    if (IsPlayingInput()) {
        ASSERT(current_byte + sizeof(ControllerState) <= recorded_input.size());
        Play(Fargs...);
        CheckInputEnd();
    } else if (IsRecordingInput()) {
        Record(Fargs...);
    }
}

void Movie::HandlePadAndCircleStatus(Service::HID::PadState& pad_state, s16& circle_pad_x,
                                     s16& circle_pad_y) {
    Handle(pad_state, circle_pad_x, circle_pad_y);
}

void Movie::HandleTouchStatus(Service::HID::TouchDataEntry& touch_data) {
    Handle(touch_data);
}

void Movie::HandleAccelerometerStatus(Service::HID::AccelerometerDataEntry& accelerometer_data) {
    Handle(accelerometer_data);
}

void Movie::HandleGyroscopeStatus(Service::HID::GyroscopeDataEntry& gyroscope_data) {
    Handle(gyroscope_data);
}

void Movie::HandleIrRst(Service::IR::PadState& pad_state, s16& c_stick_x, s16& c_stick_y) {
    Handle(pad_state, c_stick_x, c_stick_y);
}

void Movie::HandleExtraHidResponse(Service::IR::ExtraHIDResponse& extra_hid_response) {
    Handle(extra_hid_response);
}
} // namespace Core
