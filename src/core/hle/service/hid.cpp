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
    cmd_buff[4] = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventA");
    cmd_buff[5] = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventB");
    cmd_buff[6] = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventC");
    cmd_buff[7] = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventGyroscope");
    cmd_buff[8] = Kernel::CreateEvent(RESETTYPE_ONESHOT, "HID_User:EventD");

    DEBUG_LOG(KERNEL, "called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x000A0000, GetIPCHandles, "GetIPCHandles"},
    {0x00110000, nullptr,       "EnableAccelerometer"},
    {0x00130000, nullptr,       "EnableGyroscopeLow"},
    {0x00150000, nullptr,       "GetGyroscopeLowRawToDpsCoefficient"},
    {0x00160000, nullptr,       "GetGyroscopeLowCalibrateParam"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    g_shared_mem = Kernel::CreateSharedMemory("HID_User:SharedMem"); // Create shared memory object

    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}

Interface::~Interface() {
}

} // namespace
