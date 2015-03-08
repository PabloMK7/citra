// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/service.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/hid/hid_spvr.h"
#include "core/hle/service/hid/hid_user.h"

#include "core/arm/arm_interface.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/hle.h"

namespace Service {
namespace HID {

Kernel::SharedPtr<Kernel::SharedMemory> g_shared_mem = nullptr;

Kernel::SharedPtr<Kernel::Event> g_event_pad_or_touch_1;
Kernel::SharedPtr<Kernel::Event> g_event_pad_or_touch_2;
Kernel::SharedPtr<Kernel::Event> g_event_accelerometer;
Kernel::SharedPtr<Kernel::Event> g_event_gyroscope;
Kernel::SharedPtr<Kernel::Event> g_event_debug_pad;

// Next Pad state update information
static PadState next_state = {{0}};
static u32 next_pad_index = 0;
static s16 next_pad_circle_x = 0;
static s16 next_pad_circle_y = 0;

static u32 next_touch_index = 0;
static u16 next_touch_x = 0;
static u16 next_touch_y = 0;

/**
 * Gets a pointer to the PadData structure inside HID shared memory
 */
static inline SharedMem* GetSharedMem() {
    if (g_shared_mem == nullptr)
        return nullptr;
    return reinterpret_cast<SharedMem*>(g_shared_mem->GetPointer().ValueOr(nullptr));
}

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

/**
 * Circle Pad from keys.
 *
 * This is implemented as "pushed all the way to an edge (max) or centered (0)".
 *
 * Indicate the circle pad is pushed completely to the edge in 1 of 8 directions.
 */
static void UpdateNextCirclePadState() {
    static const s16 max_value = 0x9C;
    next_pad_circle_x = next_state.circle_left ? -max_value : 0x0;
    next_pad_circle_x += next_state.circle_right ? max_value : 0x0;
    next_pad_circle_y = next_state.circle_down ? -max_value : 0x0;
    next_pad_circle_y += next_state.circle_up ? max_value : 0x0;
}

void PadButtonPress(const PadState& pad_state) {
    next_state.hex |= pad_state.hex;
    UpdateNextCirclePadState();
}

void PadButtonRelease(const PadState& pad_state) {
    next_state.hex &= ~pad_state.hex;
    UpdateNextCirclePadState();
}

void PadUpdateComplete() {
    SharedMem* shared_mem = GetSharedMem();

    if (shared_mem == nullptr)
        return;

    shared_mem->pad.current_state.hex = next_state.hex;
    shared_mem->pad.index = next_pad_index;
    next_pad_index = (next_pad_index + 1) % shared_mem->pad.entries.size();

    // Get the previous Pad state
    u32 last_entry_index = (shared_mem->pad.index - 1) % shared_mem->pad.entries.size();
    PadState old_state = shared_mem->pad.entries[last_entry_index].current_state;

    // Compute bitmask with 1s for bits different from the old state
    PadState changed;
    changed.hex = (next_state.hex ^ old_state.hex);

    // Compute what was added
    PadState additions;
    additions.hex = changed.hex & next_state.hex;

    // Compute what was removed
    PadState removals;
    removals.hex = changed.hex & old_state.hex;

    // Get the current Pad entry
    PadDataEntry* current_pad_entry = &shared_mem->pad.entries[shared_mem->pad.index];

    // Update entry properties
    current_pad_entry->current_state.hex = next_state.hex;
    current_pad_entry->delta_additions.hex = additions.hex;
    current_pad_entry->delta_removals.hex = removals.hex;

    // Set circle Pad
    current_pad_entry->circle_pad_x = next_pad_circle_x;
    current_pad_entry->circle_pad_y = next_pad_circle_y;

    // If we just updated index 0, provide a new timestamp
    if (shared_mem->pad.index == 0) {
        shared_mem->pad.index_reset_ticks_previous = shared_mem->pad.index_reset_ticks;
        shared_mem->pad.index_reset_ticks = (s64)Core::g_app_core->GetTicks();
    }

    // Signal both handles when there's an update to Pad or touch
    g_event_pad_or_touch_1->Signal();
    g_event_pad_or_touch_2->Signal();
}

void TouchPress(u16 x, u16 y) {
    next_touch_x = x;
    next_touch_y = y;
}

void TouchRelease() {
    next_touch_x = 0;
    next_touch_y = 0;
}

void TouchUpdateComplete() {
    SharedMem* shared_mem = GetSharedMem();

    if (shared_mem == nullptr)
        return;

    shared_mem->touch.index = next_touch_index;
    next_touch_index = (next_touch_index + 1) % shared_mem->touch.entries.size();

    // Get the current touch entry
    TouchDataEntry* current_touch_entry = &shared_mem->touch.entries[shared_mem->touch.index];

    // Set touchpad position
    current_touch_entry->x = next_touch_x;
    current_touch_entry->y = next_touch_y;

    // TODO(bunnei): Verify this behavior on real hardware
    current_touch_entry->data_valid = (next_touch_x || next_touch_y) ? 1 : 0;

    // TODO(bunnei): We're not doing anything with offset 0xA8 + 0x18 of HID SharedMemory, which
    // supposedly is "Touch-screen entry, which contains the raw coordinate data prior to being
    // converted to pixel coordinates." (http://3dbrew.org/wiki/HID_Shared_Memory#Offset_0xA8).

    // If we just updated index 0, provide a new timestamp
    if (shared_mem->touch.index == 0) {
        shared_mem->touch.index_reset_ticks_previous = shared_mem->touch.index_reset_ticks;
        shared_mem->touch.index_reset_ticks = (s64)Core::g_app_core->GetTicks();
    }
    
    // Signal both handles when there's an update to Pad or touch
    g_event_pad_or_touch_1->Signal();
    g_event_pad_or_touch_2->Signal();
}

void GetIPCHandles(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = 0; // No error
    // TODO(yuriks): Return error from SendSyncRequest is this fails (part of IPC marshalling)
    cmd_buff[3] = Kernel::g_handle_table.Create(Service::HID::g_shared_mem).MoveFrom();
    cmd_buff[4] = Kernel::g_handle_table.Create(Service::HID::g_event_pad_or_touch_1).MoveFrom();
    cmd_buff[5] = Kernel::g_handle_table.Create(Service::HID::g_event_pad_or_touch_2).MoveFrom();
    cmd_buff[6] = Kernel::g_handle_table.Create(Service::HID::g_event_accelerometer).MoveFrom();
    cmd_buff[7] = Kernel::g_handle_table.Create(Service::HID::g_event_gyroscope).MoveFrom();
    cmd_buff[8] = Kernel::g_handle_table.Create(Service::HID::g_event_debug_pad).MoveFrom();
}

void HIDInit() {
    using namespace Kernel;

    AddService(new HID_U_Interface);
    AddService(new HID_SPVR_Interface);

    g_shared_mem = SharedMemory::Create("HID:SharedMem");

    // Create event handles
    g_event_pad_or_touch_1 = Event::Create(RESETTYPE_ONESHOT, "HID:EventPadOrTouch1");
    g_event_pad_or_touch_2 = Event::Create(RESETTYPE_ONESHOT, "HID:EventPadOrTouch2");
    g_event_accelerometer  = Event::Create(RESETTYPE_ONESHOT, "HID:EventAccelerometer");
    g_event_gyroscope      = Event::Create(RESETTYPE_ONESHOT, "HID:EventGyroscope");
    g_event_debug_pad      = Event::Create(RESETTYPE_ONESHOT, "HID:EventDebugPad");
}

void HIDShutdown() {

}

}
}
