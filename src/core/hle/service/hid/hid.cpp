// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>

#include "common/logging/log.h"
#include "common/emu_window.h"

#include "core/hle/service/service.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/hid/hid_spvr.h"
#include "core/hle/service/hid/hid_user.h"

#include "core/core_timing.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"

#include "video_core/video_core.h"

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

static int enable_accelerometer_count = 0; // positive means enabled
static int enable_gyroscope_count = 0; // positive means enabled

static PadState GetCirclePadDirectionState(s16 circle_pad_x, s16 circle_pad_y) {
    constexpr float TAN30 = 0.577350269, TAN60 = 1 / TAN30; // 30 degree and 60 degree are angular thresholds for directions
    constexpr int CIRCLE_PAD_THRESHOLD_SQUARE = 40 * 40; // a circle pad radius greater than 40 will trigger circle pad direction
    PadState state;
    state.hex = 0;

    if (circle_pad_x * circle_pad_x + circle_pad_y * circle_pad_y > CIRCLE_PAD_THRESHOLD_SQUARE) {
        float t = std::abs(static_cast<float>(circle_pad_y) / circle_pad_x);

        if (circle_pad_x != 0 && t < TAN60) {
            if (circle_pad_x > 0)
                state.circle_right.Assign(1);
            else
                state.circle_left.Assign(1);
        }

        if (circle_pad_x == 0 || t > TAN30) {
            if (circle_pad_y > 0)
                state.circle_up.Assign(1);
            else
                state.circle_down.Assign(1);
        }
    }

    return state;
}

void Update() {
    SharedMem* mem = reinterpret_cast<SharedMem*>(shared_mem->GetPointer());

    if (mem == nullptr) {
        LOG_DEBUG(Service_HID, "Cannot update HID prior to mapping shared memory!");
        return;
    }

    PadState state = VideoCore::g_emu_window->GetPadState();

    // Get current circle pad position and update circle pad direction
    s16 circle_pad_x, circle_pad_y;
    std::tie(circle_pad_x, circle_pad_y) = VideoCore::g_emu_window->GetCirclePadState();
    state.hex |= GetCirclePadDirectionState(circle_pad_x, circle_pad_y).hex;

    mem->pad.current_state.hex = state.hex;
    mem->pad.index = next_pad_index;
    next_pad_index = (next_pad_index + 1) % mem->pad.entries.size();

    // Get the previous Pad state
    u32 last_entry_index = (mem->pad.index - 1) % mem->pad.entries.size();
    PadState old_state = mem->pad.entries[last_entry_index].current_state;

    // Compute bitmask with 1s for bits different from the old state
    PadState changed = { { (state.hex ^ old_state.hex) } };

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

    std::tie(touch_entry.x, touch_entry.y, pressed) = VideoCore::g_emu_window->GetTouchState();
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

    // Update accelerometer
    if (enable_accelerometer_count > 0) {
        mem->accelerometer.index = next_accelerometer_index;
        next_accelerometer_index = (next_accelerometer_index + 1) % mem->accelerometer.entries.size();

        AccelerometerDataEntry& accelerometer_entry = mem->accelerometer.entries[mem->accelerometer.index];
        std::tie(accelerometer_entry.x, accelerometer_entry.y, accelerometer_entry.z)
            = VideoCore::g_emu_window->GetAccelerometerState();

        // Make up "raw" entry
        // TODO(wwylele):
        // From hardware testing, the raw_entry values are approximately,
        // but not exactly, as twice as corresponding entries (or with a minus sign).
        // It may caused by system calibration to the accelerometer.
        // Figure out how it works, or, if no game reads raw_entry,
        // the following three lines can be removed and leave raw_entry unimplemented.
        mem->accelerometer.raw_entry.x = -2 * accelerometer_entry.x;
        mem->accelerometer.raw_entry.z = 2 * accelerometer_entry.y;
        mem->accelerometer.raw_entry.y = -2 * accelerometer_entry.z;

        // If we just updated index 0, provide a new timestamp
        if (mem->accelerometer.index == 0) {
            mem->accelerometer.index_reset_ticks_previous = mem->accelerometer.index_reset_ticks;
            mem->accelerometer.index_reset_ticks = (s64)CoreTiming::GetTicks();
        }

        event_accelerometer->Signal();
    }

    // Update gyroscope
    if (enable_gyroscope_count > 0) {
        mem->gyroscope.index = next_gyroscope_index;
        next_gyroscope_index = (next_gyroscope_index + 1) % mem->gyroscope.entries.size();

        GyroscopeDataEntry& gyroscope_entry = mem->gyroscope.entries[mem->gyroscope.index];
        std::tie(gyroscope_entry.x, gyroscope_entry.y, gyroscope_entry.z)
            = VideoCore::g_emu_window->GetGyroscopeState();

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
    }
}

void GetIPCHandles(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = 0; // No error
    cmd_buff[2] = 0x14000000; // IPC Command Structure translate-header
    // TODO(yuriks): Return error from SendSyncRequest is this fails (part of IPC marshalling)
    cmd_buff[3] = Kernel::g_handle_table.Create(Service::HID::shared_mem).MoveFrom();
    cmd_buff[4] = Kernel::g_handle_table.Create(Service::HID::event_pad_or_touch_1).MoveFrom();
    cmd_buff[5] = Kernel::g_handle_table.Create(Service::HID::event_pad_or_touch_2).MoveFrom();
    cmd_buff[6] = Kernel::g_handle_table.Create(Service::HID::event_accelerometer).MoveFrom();
    cmd_buff[7] = Kernel::g_handle_table.Create(Service::HID::event_gyroscope).MoveFrom();
    cmd_buff[8] = Kernel::g_handle_table.Create(Service::HID::event_debug_pad).MoveFrom();
}

void EnableAccelerometer(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ++enable_accelerometer_count;
    event_accelerometer->Signal();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_HID, "called");
}

void DisableAccelerometer(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    --enable_accelerometer_count;
    event_accelerometer->Signal();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_HID, "called");
}

void EnableGyroscopeLow(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    ++enable_gyroscope_count;
    event_gyroscope->Signal();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_HID, "called");
}

void DisableGyroscopeLow(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    --enable_gyroscope_count;
    event_gyroscope->Signal();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_HID, "called");
}

void GetGyroscopeLowRawToDpsCoefficient(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    f32 coef = VideoCore::g_emu_window->GetGyroscopeRawToDpsCoefficient();
    memcpy(&cmd_buff[2], &coef, 4);
}

void GetGyroscopeLowCalibrateParam(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    const s16 param_unit = 6700; // an approximate value taken from hw
    GyroscopeCalibrateParam param = {
        { 0, param_unit, -param_unit },
        { 0, param_unit, -param_unit },
        { 0, param_unit, -param_unit },
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

    using Kernel::MemoryPermission;
    shared_mem = SharedMemory::Create(nullptr, 0x1000,
                                      MemoryPermission::ReadWrite, MemoryPermission::Read,
                                      0, Kernel::MemoryRegion::BASE, "HID:SharedMemory");

    next_pad_index = 0;
    next_touch_index = 0;

    // Create event handles
    event_pad_or_touch_1 = Event::Create(ResetType::OneShot, "HID:EventPadOrTouch1");
    event_pad_or_touch_2 = Event::Create(ResetType::OneShot, "HID:EventPadOrTouch2");
    event_accelerometer  = Event::Create(ResetType::OneShot, "HID:EventAccelerometer");
    event_gyroscope      = Event::Create(ResetType::OneShot, "HID:EventGyroscope");
    event_debug_pad      = Event::Create(ResetType::OneShot, "HID:EventDebugPad");
}

void Shutdown() {
    shared_mem = nullptr;
    event_pad_or_touch_1 = nullptr;
    event_pad_or_touch_2 = nullptr;
    event_accelerometer = nullptr;
    event_gyroscope = nullptr;
    event_debug_pad = nullptr;
}

} // namespace HID

} // namespace Service
