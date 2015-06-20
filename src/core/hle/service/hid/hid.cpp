// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "common/emu_window.h"

#include "core/hle/service/service.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/hid/hid_spvr.h"
#include "core/hle/service/hid/hid_user.h"

#include "core/core_timing.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/hle.h"

#include "video_core/video_core.h"

namespace Service {
namespace HID {

static const int MAX_CIRCLEPAD_POS = 0x9C; ///< Max value for a circle pad position

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

const std::array<Service::HID::PadState, Settings::NativeInput::NUM_INPUTS> pad_mapping = {
    Service::HID::PAD_A, Service::HID::PAD_B, Service::HID::PAD_X, Service::HID::PAD_Y,
    Service::HID::PAD_L, Service::HID::PAD_R, Service::HID::PAD_ZL, Service::HID::PAD_ZR,
    Service::HID::PAD_START, Service::HID::PAD_SELECT, Service::HID::PAD_NONE,
    Service::HID::PAD_UP, Service::HID::PAD_DOWN, Service::HID::PAD_LEFT, Service::HID::PAD_RIGHT,
    Service::HID::PAD_CIRCLE_UP, Service::HID::PAD_CIRCLE_DOWN, Service::HID::PAD_CIRCLE_LEFT, Service::HID::PAD_CIRCLE_RIGHT,
    Service::HID::PAD_C_UP, Service::HID::PAD_C_DOWN, Service::HID::PAD_C_LEFT, Service::HID::PAD_C_RIGHT
};


// TODO(peachum):
// Add a method for setting analog input from joystick device for the circle Pad.
//
// This method should:
//     * Be called after both PadButton<Press, Release>().
//     * Be called before PadUpdateComplete()
//     * Set current PadEntry.circle_pad_<axis> using analog data
//     * Set PadData.raw_circle_pad_data
//     * Set PadData.current_state.circle_right = 1 if current PadEntry.circle_pad_x >= 41
//     * Set PadData.current_state.circle_up = 1 if current PadEntry.circle_pad_y >= 41
//     * Set PadData.current_state.circle_left = 1 if current PadEntry.circle_pad_x <= -41
//     * Set PadData.current_state.circle_right = 1 if current PadEntry.circle_pad_y <= -41

void Update() {
    SharedMem* mem = reinterpret_cast<SharedMem*>(shared_mem->GetPointer());
    const PadState state = VideoCore::g_emu_window->GetPadState();

    if (mem == nullptr) {
        LOG_DEBUG(Service_HID, "Cannot update HID prior to mapping shared memory!");
        return;
    }

    mem->pad.current_state.hex = state.hex;
    mem->pad.index = next_pad_index;
    next_touch_index = (next_touch_index + 1) % mem->pad.entries.size();

    // Get the previous Pad state
    u32 last_entry_index = (mem->pad.index - 1) % mem->pad.entries.size();
    PadState old_state = mem->pad.entries[last_entry_index].current_state;

    // Compute bitmask with 1s for bits different from the old state
    PadState changed = { { (state.hex ^ old_state.hex) } };

    // Get the current Pad entry
    PadDataEntry* pad_entry = &mem->pad.entries[mem->pad.index];

    // Update entry properties
    pad_entry->current_state.hex = state.hex;
    pad_entry->delta_additions.hex = changed.hex & state.hex;
    pad_entry->delta_removals.hex = changed.hex & old_state.hex;;

    // Set circle Pad
    pad_entry->circle_pad_x = state.circle_left  ? -MAX_CIRCLEPAD_POS :
                              state.circle_right ?  MAX_CIRCLEPAD_POS : 0x0;
    pad_entry->circle_pad_y = state.circle_down  ? -MAX_CIRCLEPAD_POS :
                              state.circle_up    ?  MAX_CIRCLEPAD_POS : 0x0;

    // If we just updated index 0, provide a new timestamp
    if (mem->pad.index == 0) {
        mem->pad.index_reset_ticks_previous = mem->pad.index_reset_ticks;
        mem->pad.index_reset_ticks = (s64)CoreTiming::GetTicks();
    }

    mem->touch.index = next_touch_index;
    next_touch_index = (next_touch_index + 1) % mem->touch.entries.size();

    // Get the current touch entry
    TouchDataEntry* touch_entry = &mem->touch.entries[mem->touch.index];
    bool pressed = false;

    std::tie(touch_entry->x, touch_entry->y, pressed) = VideoCore::g_emu_window->GetTouchState();
    touch_entry->valid = pressed ? 1 : 0;

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

    event_accelerometer->Signal();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_HID, "(STUBBED) called");
}

void DisableAccelerometer(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    event_accelerometer->Signal();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_HID, "(STUBBED) called");
}

void EnableGyroscopeLow(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    event_gyroscope->Signal();

    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_HID, "(STUBBED) called");
}

void DisableGyroscopeLow(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    event_gyroscope->Signal();

    cmd_buff[1] = RESULT_SUCCESS.raw;

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
    shared_mem = SharedMemory::Create(0x1000, MemoryPermission::ReadWrite,
            MemoryPermission::Read, "HID:SharedMem");

    next_pad_index = 0;
    next_touch_index = 0;

    // Create event handles
    event_pad_or_touch_1 = Event::Create(RESETTYPE_ONESHOT, "HID:EventPadOrTouch1");
    event_pad_or_touch_2 = Event::Create(RESETTYPE_ONESHOT, "HID:EventPadOrTouch2");
    event_accelerometer  = Event::Create(RESETTYPE_ONESHOT, "HID:EventAccelerometer");
    event_gyroscope      = Event::Create(RESETTYPE_ONESHOT, "HID:EventGyroscope");
    event_debug_pad      = Event::Create(RESETTYPE_ONESHOT, "HID:EventDebugPad");
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
