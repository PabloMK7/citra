// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"

#include "core/arm/arm_interface.h"
#include "core/hle/hle.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "hid_user.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace HID_User

namespace HID_User {

// Handle to shared memory region designated to HID_User service
static Handle shared_mem = 0;

// Event handles
static Handle event_pad_or_touch_1 = 0;
static Handle event_pad_or_touch_2 = 0;
static Handle event_accelerometer = 0;
static Handle event_gyroscope = 0;
static Handle event_debug_pad = 0;

// Next Pad state update information
static PadState next_state = {{0}};
static u32 next_index = 0;
static s16 next_circle_x = 0;
static s16 next_circle_y = 0;

/**
 * Gets a pointer to the PadData structure inside HID shared memory
 */
static inline PadData* GetPadData() {
    return reinterpret_cast<PadData*>(Kernel::GetSharedMemoryPointer(shared_mem, 0).ValueOr(nullptr));
}

/**
 * Circle Pad from keys.
 *
 * This is implemented as "pushed all the way to an edge (max) or centered (0)".
 *
 * Indicate the circle pad is pushed completely to the edge in 1 of 8 directions.
 */
static void UpdateNextCirclePadState() {
    static const s16 max_value = 0x9C;
    next_circle_x = next_state.circle_left ? -max_value : 0x0;
    next_circle_x += next_state.circle_right ? max_value : 0x0;
    next_circle_y = next_state.circle_down ? -max_value : 0x0;
    next_circle_y += next_state.circle_up ? max_value : 0x0;
}

/**
 * Sets a Pad state (button or button combo) as pressed
 */
void PadButtonPress(const PadState& pad_state) {
    next_state.hex |= pad_state.hex;
    UpdateNextCirclePadState();
}

/**
 * Sets a Pad state (button or button combo) as released
 */
void PadButtonRelease(const PadState& pad_state) {
    next_state.hex &= ~pad_state.hex;
    UpdateNextCirclePadState();
}

/**
 * Called after all Pad changes to be included in this update have been made,
 * including both Pad key changes and analog circle Pad changes.
 */
void PadUpdateComplete() {
    PadData* pad_data = GetPadData();

    if (pad_data == nullptr) {
        return;
    }

    // Update PadData struct
    pad_data->current_state.hex = next_state.hex;
    pad_data->index = next_index;
    next_index = (next_index + 1) % pad_data->entries.size();

    // Get the previous Pad state
    u32 last_entry_index = (pad_data->index - 1) % pad_data->entries.size();
    PadState old_state = pad_data->entries[last_entry_index].current_state;

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
    PadDataEntry* current_pad_entry = &pad_data->entries[pad_data->index];

    // Update entry properties
    current_pad_entry->current_state.hex = next_state.hex;
    current_pad_entry->delta_additions.hex = additions.hex;
    current_pad_entry->delta_removals.hex = removals.hex;

    // Set circle Pad
    current_pad_entry->circle_pad_x = next_circle_x;
    current_pad_entry->circle_pad_y = next_circle_y;

    // If we just updated index 0, provide a new timestamp
    if (pad_data->index == 0) {
        pad_data->index_reset_ticks_previous = pad_data->index_reset_ticks;
        pad_data->index_reset_ticks = (s64)Core::g_app_core->GetTicks();
    }

    // Signal both handles when there's an update to Pad or touch
    Kernel::SignalEvent(event_pad_or_touch_1);
    Kernel::SignalEvent(event_pad_or_touch_2);
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
 * HID_User::GetIPCHandles service function
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
void GetIPCHandles(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = 0; // No error
    cmd_buff[3] = shared_mem;
    cmd_buff[4] = event_pad_or_touch_1;
    cmd_buff[5] = event_pad_or_touch_2;
    cmd_buff[6] = event_accelerometer;
    cmd_buff[7] = event_gyroscope;
    cmd_buff[8] = event_debug_pad;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x000A0000, GetIPCHandles, "GetIPCHandles"},
    {0x000B0000, nullptr,       "StartAnalogStickCalibration"},
    {0x000E0000, nullptr,       "GetAnalogStickCalibrateParam"},
    {0x00110000, nullptr,       "EnableAccelerometer"},
    {0x00120000, nullptr,       "DisableAccelerometer"},
    {0x00130000, nullptr,       "EnableGyroscopeLow"},
    {0x00140000, nullptr,       "DisableGyroscopeLow"},
    {0x00150000, nullptr,       "GetGyroscopeLowRawToDpsCoefficient"},
    {0x00160000, nullptr,       "GetGyroscopeLowCalibrateParam"},
    {0x00170000, nullptr,       "GetSoundVolume"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    shared_mem = Kernel::CreateSharedMemory("HID_User:SharedMem"); // Create shared memory object

    // Create event handles
    event_pad_or_touch_1 = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventPadOrTouch1");
    event_pad_or_touch_2 = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventPadOrTouch2");
    event_accelerometer = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventAccelerometer");
    event_gyroscope = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventGyroscope");
    event_debug_pad = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventDebugPad");

    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

} // namespace
