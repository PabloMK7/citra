// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <cmath>
#include <memory>
#include "common/logging/log.h"
#include "core/3ds.h"
#include "core/core.h"
#include "core/core_timing.h"
#include "core/frontend/input.h"
#include "core/hle/ipc.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/handle_table.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/hid/hid_spvr.h"
#include "core/hle/service/hid/hid_user.h"
#include "core/hle/service/service.h"

namespace Service {
namespace HID {

// Handle to shared memory region designated to HID_User service
static Kernel::SharedPtr<Kernel::SharedMemory> shared_mem;

// Event handles
static Kernel::SharedPtr<Kernel::Event> event_pad_or_touch_1;
static Kernel::SharedPtr<Kernel::Event> event_pad_or_touch_2;
static Kernel::SharedPtr<Kernel::Event> event_accelerometer;
static Kernel::SharedPtr<Kernel::Event> event_gyroscope;
static Kernel::SharedPtr<Kernel::Event> event_debug_pad;

static u32 next_pad_index;
static u32 next_touch_index;
static u32 next_accelerometer_index;
static u32 next_gyroscope_index;

static int enable_accelerometer_count; // positive means enabled
static int enable_gyroscope_count;     // positive means enabled

static int pad_update_event;
static int accelerometer_update_event;
static int gyroscope_update_event;

// Updating period for each HID device. These empirical values are measured from a 11.2 3DS.
constexpr u64 pad_update_ticks = BASE_CLOCK_RATE_ARM11 / 234;
constexpr u64 accelerometer_update_ticks = BASE_CLOCK_RATE_ARM11 / 104;
constexpr u64 gyroscope_update_ticks = BASE_CLOCK_RATE_ARM11 / 101;

constexpr float accelerometer_coef = 512.0f; // measured from hw test result
constexpr float gyroscope_coef = 14.375f; // got from hwtest GetGyroscopeLowRawToDpsCoefficient call

static std::atomic<bool> is_device_reload_pending;
static std::array<std::unique_ptr<Input::ButtonDevice>, Settings::NativeButton::NUM_BUTTONS_HID>
    buttons;
static std::unique_ptr<Input::AnalogDevice> circle_pad;
static std::unique_ptr<Input::MotionDevice> motion_device;
static std::unique_ptr<Input::TouchDevice> touch_device;

DirectionState GetStickDirectionState(s16 circle_pad_x, s16 circle_pad_y) {
    // 30 degree and 60 degree are angular thresholds for directions
    constexpr float TAN30 = 0.577350269f;
    constexpr float TAN60 = 1 / TAN30;
    // a circle pad radius greater than 40 will trigger circle pad direction
    constexpr int CIRCLE_PAD_THRESHOLD_SQUARE = 40 * 40;
    DirectionState state{false, false, false, false};

    if (circle_pad_x * circle_pad_x + circle_pad_y * circle_pad_y > CIRCLE_PAD_THRESHOLD_SQUARE) {
        float t = std::abs(static_cast<float>(circle_pad_y) / circle_pad_x);

        if (circle_pad_x != 0 && t < TAN60) {
            if (circle_pad_x > 0)
                state.right = true;
            else
                state.left = true;
        }

        if (circle_pad_x == 0 || t > TAN30) {
            if (circle_pad_y > 0)
                state.up = true;
            else
                state.down = true;
        }
    }

    return state;
}

static void LoadInputDevices() {
    std::transform(Settings::values.buttons.begin() + Settings::NativeButton::BUTTON_HID_BEGIN,
                   Settings::values.buttons.begin() + Settings::NativeButton::BUTTON_HID_END,
                   buttons.begin(), Input::CreateDevice<Input::ButtonDevice>);
    circle_pad = Input::CreateDevice<Input::AnalogDevice>(
        Settings::values.analogs[Settings::NativeAnalog::CirclePad]);
    motion_device = Input::CreateDevice<Input::MotionDevice>(Settings::values.motion_device);
    touch_device = Input::CreateDevice<Input::TouchDevice>(Settings::values.touch_device);
}

static void UnloadInputDevices() {
    for (auto& button : buttons) {
        button.reset();
    }
    circle_pad.reset();
    motion_device.reset();
    touch_device.reset();
}

static void UpdatePadCallback(u64 userdata, int cycles_late) {
    SharedMem* mem = reinterpret_cast<SharedMem*>(shared_mem->GetPointer());

    if (is_device_reload_pending.exchange(false))
        LoadInputDevices();

    PadState state;
    using namespace Settings::NativeButton;
    state.a.Assign(buttons[A - BUTTON_HID_BEGIN]->GetStatus());
    state.b.Assign(buttons[B - BUTTON_HID_BEGIN]->GetStatus());
    state.x.Assign(buttons[X - BUTTON_HID_BEGIN]->GetStatus());
    state.y.Assign(buttons[Y - BUTTON_HID_BEGIN]->GetStatus());
    state.right.Assign(buttons[Right - BUTTON_HID_BEGIN]->GetStatus());
    state.left.Assign(buttons[Left - BUTTON_HID_BEGIN]->GetStatus());
    state.up.Assign(buttons[Up - BUTTON_HID_BEGIN]->GetStatus());
    state.down.Assign(buttons[Down - BUTTON_HID_BEGIN]->GetStatus());
    state.l.Assign(buttons[L - BUTTON_HID_BEGIN]->GetStatus());
    state.r.Assign(buttons[R - BUTTON_HID_BEGIN]->GetStatus());
    state.start.Assign(buttons[Start - BUTTON_HID_BEGIN]->GetStatus());
    state.select.Assign(buttons[Select - BUTTON_HID_BEGIN]->GetStatus());

    // Get current circle pad position and update circle pad direction
    float circle_pad_x_f, circle_pad_y_f;
    std::tie(circle_pad_x_f, circle_pad_y_f) = circle_pad->GetStatus();
    constexpr int MAX_CIRCLEPAD_POS = 0x9C; // Max value for a circle pad position
    s16 circle_pad_x = static_cast<s16>(circle_pad_x_f * MAX_CIRCLEPAD_POS);
    s16 circle_pad_y = static_cast<s16>(circle_pad_y_f * MAX_CIRCLEPAD_POS);
    const DirectionState direction = GetStickDirectionState(circle_pad_x, circle_pad_y);
    state.circle_up.Assign(direction.up);
    state.circle_down.Assign(direction.down);
    state.circle_left.Assign(direction.left);
    state.circle_right.Assign(direction.right);

    mem->pad.current_state.hex = state.hex;
    mem->pad.index = next_pad_index;
    next_pad_index = (next_pad_index + 1) % mem->pad.entries.size();

    // Get the previous Pad state
    u32 last_entry_index = (mem->pad.index - 1) % mem->pad.entries.size();
    PadState old_state = mem->pad.entries[last_entry_index].current_state;

    // Compute bitmask with 1s for bits different from the old state
    PadState changed = {{(state.hex ^ old_state.hex)}};

    // Get the current Pad entry
    PadDataEntry& pad_entry = mem->pad.entries[mem->pad.index];

    // Update entry properties
    pad_entry.current_state.hex = state.hex;
    pad_entry.delta_additions.hex = changed.hex & state.hex;
    pad_entry.delta_removals.hex = changed.hex & old_state.hex;
    pad_entry.circle_pad_x = circle_pad_x;
    pad_entry.circle_pad_y = circle_pad_y;

    // If we just updated index 0, provide a new timestamp
    if (mem->pad.index == 0) {
        mem->pad.index_reset_ticks_previous = mem->pad.index_reset_ticks;
        mem->pad.index_reset_ticks = (s64)CoreTiming::GetTicks();
    }

    mem->touch.index = next_touch_index;
    next_touch_index = (next_touch_index + 1) % mem->touch.entries.size();

    // Get the current touch entry
    TouchDataEntry& touch_entry = mem->touch.entries[mem->touch.index];
    bool pressed = false;
    float x, y;
    std::tie(x, y, pressed) = touch_device->GetStatus();
    touch_entry.x = static_cast<u16>(x * Core::kScreenBottomWidth);
    touch_entry.y = static_cast<u16>(y * Core::kScreenBottomHeight);
    touch_entry.valid.Assign(pressed ? 1 : 0);

    // TODO(bunnei): We're not doing anything with offset 0xA8 + 0x18 of HID SharedMemory, which
    // supposedly is "Touch-screen entry, which contains the raw coordinate data prior to being
    // converted to pixel coordinates." (http://3dbrew.org/wiki/HID_Shared_Memory#Offset_0xA8).

    // If we just updated index 0, provide a new timestamp
    if (mem->touch.index == 0) {
        mem->touch.index_reset_ticks_previous = mem->touch.index_reset_ticks;
        mem->touch.index_reset_ticks = (s64)CoreTiming::GetTicks();
    }

    // Signal both handles when there's an update to Pad or touch
    event_pad_or_touch_1->Signal();
    event_pad_or_touch_2->Signal();

    // Reschedule recurrent event
    CoreTiming::ScheduleEvent(pad_update_ticks - cycles_late, pad_update_event);
}

static void UpdateAccelerometerCallback(u64 userdata, int cycles_late) {
    SharedMem* mem = reinterpret_cast<SharedMem*>(shared_mem->GetPointer());

    mem->accelerometer.index = next_accelerometer_index;
    next_accelerometer_index = (next_accelerometer_index + 1) % mem->accelerometer.entries.size();

    Math::Vec3<float> accel;
    std::tie(accel, std::ignore) = motion_device->GetStatus();
    accel *= accelerometer_coef;
    // TODO(wwylele): do a time stretch like the one in UpdateGyroscopeCallback
    // The time stretch formula should be like
    // stretched_vector = (raw_vector - gravity) * stretch_ratio + gravity

    AccelerometerDataEntry& accelerometer_entry =
        mem->accelerometer.entries[mem->accelerometer.index];

    accelerometer_entry.x = static_cast<s16>(accel.x);
    accelerometer_entry.y = static_cast<s16>(accel.y);
    accelerometer_entry.z = static_cast<s16>(accel.z);

    // Make up "raw" entry
    // TODO(wwylele):
    // From hardware testing, the raw_entry values are approximately, but not exactly, as twice as
    // corresponding entries (or with a minus sign). It may caused by system calibration to the
    // accelerometer. Figure out how it works, or, if no game reads raw_entry, the following three
    // lines can be removed and leave raw_entry unimplemented.
    mem->accelerometer.raw_entry.x = -2 * accelerometer_entry.x;
    mem->accelerometer.raw_entry.z = 2 * accelerometer_entry.y;
    mem->accelerometer.raw_entry.y = -2 * accelerometer_entry.z;

    // If we just updated index 0, provide a new timestamp
    if (mem->accelerometer.index == 0) {
        mem->accelerometer.index_reset_ticks_previous = mem->accelerometer.index_reset_ticks;
        mem->accelerometer.index_reset_ticks = (s64)CoreTiming::GetTicks();
    }

    event_accelerometer->Signal();

    // Reschedule recurrent event
    CoreTiming::ScheduleEvent(accelerometer_update_ticks - cycles_late, accelerometer_update_event);
}

static void UpdateGyroscopeCallback(u64 userdata, int cycles_late) {
    SharedMem* mem = reinterpret_cast<SharedMem*>(shared_mem->GetPointer());

    mem->gyroscope.index = next_gyroscope_index;
    next_gyroscope_index = (next_gyroscope_index + 1) % mem->gyroscope.entries.size();

    GyroscopeDataEntry& gyroscope_entry = mem->gyroscope.entries[mem->gyroscope.index];

    Math::Vec3<float> gyro;
    std::tie(std::ignore, gyro) = motion_device->GetStatus();
    double stretch = Core::System::GetInstance().perf_stats.GetLastFrameTimeScale();
    gyro *= gyroscope_coef * static_cast<float>(stretch);
    gyroscope_entry.x = static_cast<s16>(gyro.x);
    gyroscope_entry.y = static_cast<s16>(gyro.y);
    gyroscope_entry.z = static_cast<s16>(gyro.z);

    // Make up "raw" entry
    mem->gyroscope.raw_entry.x = gyroscope_entry.x;
    mem->gyroscope.raw_entry.z = -gyroscope_entry.y;
    mem->gyroscope.raw_entry.y = gyroscope_entry.z;

    // If we just updated index 0, provide a new timestamp
    if (mem->gyroscope.index == 0) {
        mem->gyroscope.index_reset_ticks_previous = mem->gyroscope.index_reset_ticks;
        mem->gyroscope.index_reset_ticks = (s64)CoreTiming::GetTicks();
    }

    event_gyroscope->Signal();

    // Reschedule recurrent event
    CoreTiming::ScheduleEvent(gyroscope_update_ticks - cycles_late, gyroscope_update_event);
}

void GetIPCHandles(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = 0;          // No error
    cmd_buff[2] = 0x14000000; // IPC Command Structure translate-header
    // TODO(yuriks): Return error from SendSyncRequest is this fails (part of IPC marshalling)
    cmd_buff[3] = Kernel::g_handle_table.Create(Service::HID::shared_mem).Unwrap();
    cmd_buff[4] = Kernel::g_handle_table.Create(Service::HID::event_pad_or_touch_1).Unwrap();
    cmd_buff[5] = Kernel::g_handle_table.Create(Service::HID::event_pad_or_touch_2).Unwrap();
    cmd_buff[6] = Kernel::g_handle_table.Create(Service::HID::event_accelerometer).Unwrap();
    cmd_buff[7] = Kernel::g_handle_table.Create(Service::HID::event_gyroscope).Unwrap();
    cmd_buff[8] = Kernel::g_handle_table.Create(Service::HID::event_debug_pad).Unwrap();
}

void EnableAccelerometer(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ++enable_accelerometer_count;

    // Schedules the accelerometer update event if the accelerometer was just enabled
    if (enable_accelerometer_count == 1) {
        CoreTiming::ScheduleEvent(accelerometer_update_ticks, accelerometer_update_event);
    }

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_HID, "called");
}

void DisableAccelerometer(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    --enable_accelerometer_count;

    // Unschedules the accelerometer update event if the accelerometer was just disabled
    if (enable_accelerometer_count == 0) {
        CoreTiming::UnscheduleEvent(accelerometer_update_event, 0);
    }

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_HID, "called");
}

void EnableGyroscopeLow(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ++enable_gyroscope_count;

    // Schedules the gyroscope update event if the gyroscope was just enabled
    if (enable_gyroscope_count == 1) {
        CoreTiming::ScheduleEvent(gyroscope_update_ticks, gyroscope_update_event);
    }

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_HID, "called");
}

void DisableGyroscopeLow(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    --enable_gyroscope_count;

    // Unschedules the gyroscope update event if the gyroscope was just disabled
    if (enable_gyroscope_count == 0) {
        CoreTiming::UnscheduleEvent(gyroscope_update_event, 0);
    }

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_HID, "called");
}

void GetGyroscopeLowRawToDpsCoefficient(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    f32 coef = gyroscope_coef;
    memcpy(&cmd_buff[2], &coef, 4);
}

void GetGyroscopeLowCalibrateParam(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    const s16 param_unit = 6700; // an approximate value taken from hw
    GyroscopeCalibrateParam param = {
        {0, param_unit, -param_unit}, {0, param_unit, -param_unit}, {0, param_unit, -param_unit},
    };
    memcpy(&cmd_buff[2], &param, sizeof(param));

    LOG_WARNING(Service_HID, "(STUBBED) called");
}

void GetSoundVolume(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const u8 volume = 0x3F; // TODO(purpasmart): Find out if this is the max value for the volume

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = volume;

    LOG_WARNING(Service_HID, "(STUBBED) called");
}

void Init() {
    using namespace Kernel;

    AddService(new HID_U_Interface);
    AddService(new HID_SPVR_Interface);

    is_device_reload_pending.store(true);

    using Kernel::MemoryPermission;
    shared_mem =
        SharedMemory::Create(nullptr, 0x1000, MemoryPermission::ReadWrite, MemoryPermission::Read,
                             0, Kernel::MemoryRegion::BASE, "HID:SharedMemory");

    next_pad_index = 0;
    next_touch_index = 0;
    next_accelerometer_index = 0;
    next_gyroscope_index = 0;

    enable_accelerometer_count = 0;
    enable_gyroscope_count = 0;

    // Create event handles
    event_pad_or_touch_1 = Event::Create(ResetType::OneShot, "HID:EventPadOrTouch1");
    event_pad_or_touch_2 = Event::Create(ResetType::OneShot, "HID:EventPadOrTouch2");
    event_accelerometer = Event::Create(ResetType::OneShot, "HID:EventAccelerometer");
    event_gyroscope = Event::Create(ResetType::OneShot, "HID:EventGyroscope");
    event_debug_pad = Event::Create(ResetType::OneShot, "HID:EventDebugPad");

    // Register update callbacks
    pad_update_event = CoreTiming::RegisterEvent("HID::UpdatePadCallback", UpdatePadCallback);
    accelerometer_update_event =
        CoreTiming::RegisterEvent("HID::UpdateAccelerometerCallback", UpdateAccelerometerCallback);
    gyroscope_update_event =
        CoreTiming::RegisterEvent("HID::UpdateGyroscopeCallback", UpdateGyroscopeCallback);

    CoreTiming::ScheduleEvent(pad_update_ticks, pad_update_event);
}

void Shutdown() {
    shared_mem = nullptr;
    event_pad_or_touch_1 = nullptr;
    event_pad_or_touch_2 = nullptr;
    event_accelerometer = nullptr;
    event_gyroscope = nullptr;
    event_debug_pad = nullptr;
    UnloadInputDevices();
}

void ReloadInputDevices() {
    is_device_reload_pending.store(true);
}

} // namespace HID

} // namespace Service
