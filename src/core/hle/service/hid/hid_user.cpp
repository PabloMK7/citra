// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/shared_memory.h"
#include "core/hle/service/hid/hid.h"
#include "hid_user.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace HID_User

namespace HID_User {


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
    // TODO(yuriks): Return error from SendSyncRequest is this fails (part of IPC marshalling)
    cmd_buff[3] = Kernel::g_handle_table.Create(Service::HID::g_shared_mem).MoveFrom();
    cmd_buff[4] = Service::HID::g_event_pad_or_touch_1;
    cmd_buff[5] = Service::HID::g_event_pad_or_touch_2;
    cmd_buff[6] = Service::HID::g_event_accelerometer;
    cmd_buff[7] = Service::HID::g_event_gyroscope;
    cmd_buff[8] = Service::HID::g_event_debug_pad;
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x000A0000, GetIPCHandles, "GetIPCHandles"},
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
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

} // namespace
