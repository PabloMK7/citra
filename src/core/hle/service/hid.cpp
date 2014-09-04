// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "common/log.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/hid.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace HID_User

namespace HID_User {

Handle g_shared_mem = 0; ///< Handle to shared memory region designated to HID_User service

// Event handles
Handle g_event_pad_or_touch_1 = 0;
Handle g_event_pad_or_touch_2 = 0;
Handle g_event_accelerometer = 0;
Handle g_event_gyroscope = 0;
Handle g_event_debug_pad = 0;

// Next PAD state update information
PADState g_next_state = {{0}};
u32 g_next_index = 0;
s16 g_next_circle_x = 0;
s16 g_next_circle_y = 0;

/** Gets a pointer to the PADData structure inside HID shared memory
 */
static inline PADData* GetPADData() {
    if (0 == g_shared_mem)
        return nullptr;

    return reinterpret_cast<PADData*>(Kernel::GetSharedMemoryPointer(g_shared_mem, 0));
}

/** Circle PAD from keys.
 *
 *  This is implemented as "pushed all the way to an edge (max) or centered (0)".
 *
 *  Indicate the circle pad is pushed completely to the edge in 1 of 8 directions.
 */
void UpdateNextCirclePADState() {
    static const s16 max_value = 0x9C;
    g_next_circle_x = g_next_state.circle_left ? -max_value : 0x0;
    g_next_circle_x += g_next_state.circle_right ? max_value : 0x0;
    g_next_circle_y = g_next_state.circle_down ? -max_value : 0x0;
    g_next_circle_y += g_next_state.circle_up ? max_value : 0x0;
}

/** Sets a PAD state (button or button combo) as pressed
 */
void PADButtonPress(PADState pad_state) {
    g_next_state.hex |= pad_state.hex;
    UpdateNextCirclePADState();
}

/** Sets a PAD state (button or button combo) as released
 */
void PADButtonRelease(PADState pad_state) {
    g_next_state.hex &= ~pad_state.hex;
    UpdateNextCirclePADState();
}

/** Called after all PAD changes to be included in this update have been made,
 *  including both PAD key changes and analog circle PAD changes.
 */
void PADUpdateComplete() {
    PADData* pad_data = GetPADData();

    // Update PADData struct
    pad_data->current_state.hex = g_next_state.hex;
    pad_data->index = g_next_index;
    g_next_index = (g_next_index + 1) % pad_data->entries.size();

    // Get the previous PAD state
    u32 last_entry_index = (pad_data->index - 1) % pad_data->entries.size();
    PADState old_state = pad_data->entries[last_entry_index].current_state;

    // Compute bitmask with 1s for bits different from the old state
    PADState changed;
    changed.hex = (g_next_state.hex ^ old_state.hex);

    // Compute what was added
    PADState additions;
    additions.hex = changed.hex & g_next_state.hex;

    // Compute what was removed
    PADState removals;
    removals.hex = changed.hex & old_state.hex;

    // Get the current PAD entry
    PADDataEntry* current_pad_entry = &pad_data->entries[pad_data->index];

    // Update entry properties
    current_pad_entry->current_state.hex = g_next_state.hex;
    current_pad_entry->delta_additions.hex = additions.hex;
    current_pad_entry->delta_removals.hex = removals.hex;

    // Set circle PAD
    current_pad_entry->circle_pad_x = g_next_circle_x;
    current_pad_entry->circle_pad_y = g_next_circle_y;

    // If we just updated index 0, provide a new timestamp
    if (pad_data->index == 0) {
        pad_data->index_reset_ticks_previous = pad_data->index_reset_ticks;
        pad_data->index_reset_ticks = (s64)Core::g_app_core->GetTicks();
    }

    // Signal both handles when there's an update to PAD or touch
    Kernel::SignalEvent(g_event_pad_or_touch_1);
    Kernel::SignalEvent(g_event_pad_or_touch_2);
}


// TODO(peachum):
// Add a method for setting analog input from joystick device for the circle PAD.
//
// This method should:
//     * Be called after both PADButton<Press, Release>().
//     * Be called before PADUpdateComplete()
//     * Set current PADEntry.circle_pad_<axis> using analog data
//     * Set PadData.raw_circle_pad_data
//     * Set PadData.current_state.circle_right = 1 if current PADEntry.circle_pad_x >= 41
//     * Set PadData.current_state.circle_up = 1 if current PADEntry.circle_pad_y >= 41
//     * Set PadData.current_state.circle_left = 1 if current PADEntry.circle_pad_x <= -41
//     * Set PadData.current_state.circle_right = 1 if current PADEntry.circle_pad_y <= -41


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
    u32* cmd_buff = Service::GetCommandBuffer();

    cmd_buff[1] = 0; // No error
    cmd_buff[3] = g_shared_mem;
    cmd_buff[4] = g_event_pad_or_touch_1;
    cmd_buff[5] = g_event_pad_or_touch_2;
    cmd_buff[6] = g_event_accelerometer;
    cmd_buff[7] = g_event_gyroscope;
    cmd_buff[8] = g_event_debug_pad;

    DEBUG_LOG(KERNEL, "called");
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
    g_shared_mem = Kernel::CreateSharedMemory("HID_User:SharedMem"); // Create shared memory object

    // Create event handles
    g_event_pad_or_touch_1 = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventPADOrTouch1");
    g_event_pad_or_touch_2 = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventPADOrTouch2");
    g_event_accelerometer = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventAccelerometer");
    g_event_gyroscope = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventGyroscope");
    g_event_debug_pad = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventDebugPAD");

    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
