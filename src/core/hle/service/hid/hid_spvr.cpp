// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/log.h"
#include "core/hle/hle.h"
#include "core/hle/service/hid/hid_spvr.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace HID_SPVR

namespace HID_User {
    extern void GetIPCHandles(Service::Interface* self);
}

namespace HID_SPVR {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000A0000, HID_User::GetIPCHandles,    "GetIPCHandles"},
    {0x000B0000, nullptr,                    "StartAnalogStickCalibration"},
    {0x000E0000, nullptr,                    "GetAnalogStickCalibrateParam"},
    {0x00110000, nullptr,                    "EnableAccelerometer"},
    {0x00120000, nullptr,                    "DisableAccelerometer"},
    {0x00130000, nullptr,                    "EnableGyroscopeLow"},
    {0x00140000, nullptr,                    "DisableGyroscopeLow"},
    {0x00150000, nullptr,                    "GetGyroscopeLowRawToDpsCoefficient"},
    {0x00160000, nullptr,                    "GetGyroscopeLowCalibrateParam"},
    {0x00170000, nullptr,                    "GetSoundVolume"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    Register(FunctionTable, ARRAY_SIZE(FunctionTable));
}
    
} // namespace
