// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/hid/hid.h"
#include "core/hle/service/hid/hid_user.h"

namespace Service {
namespace HID {

const Interface::FunctionInfo FunctionTable[] = {
    {0x000A0000, GetIPCHandles, "GetIPCHandles"},
    {0x000B0000, nullptr, "StartAnalogStickCalibration"},
    {0x000E0000, nullptr, "GetAnalogStickCalibrateParam"},
    {0x00110000, EnableAccelerometer, "EnableAccelerometer"},
    {0x00120000, DisableAccelerometer, "DisableAccelerometer"},
    {0x00130000, EnableGyroscopeLow, "EnableGyroscopeLow"},
    {0x00140000, DisableGyroscopeLow, "DisableGyroscopeLow"},
    {0x00150000, GetGyroscopeLowRawToDpsCoefficient, "GetGyroscopeLowRawToDpsCoefficient"},
    {0x00160000, GetGyroscopeLowCalibrateParam, "GetGyroscopeLowCalibrateParam"},
    {0x00170000, GetSoundVolume, "GetSoundVolume"},
};

HID_U_Interface::HID_U_Interface() {
    Register(FunctionTable);
}

} // namespace HID
} // namespace Service
