// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/hid/hid_spvr.h"

namespace Service::HID {

Spvr::Spvr(std::shared_ptr<Module> hid) : Module::Interface(std::move(hid), "hid:SPVR", 6) {
    static const FunctionInfo functions[] = {
        {0x00010200, nullptr, "CalibrateTouchScreen"},
        {0x00020000, nullptr, "UpdateTouchConfig"},
        {0x000A0000, &Spvr::GetIPCHandles, "GetIPCHandles"},
        {0x000B0000, nullptr, "StartAnalogStickCalibration"},
        {0x000E0000, nullptr, "GetAnalogStickCalibrateParam"},
        {0x00110000, &Spvr::EnableAccelerometer, "EnableAccelerometer"},
        {0x00120000, &Spvr::DisableAccelerometer, "DisableAccelerometer"},
        {0x00130000, &Spvr::EnableGyroscopeLow, "EnableGyroscopeLow"},
        {0x00140000, &Spvr::DisableGyroscopeLow, "DisableGyroscopeLow"},
        {0x00150000, &Spvr::GetGyroscopeLowRawToDpsCoefficient,
         "GetGyroscopeLowRawToDpsCoefficient"},
        {0x00160000, &Spvr::GetGyroscopeLowCalibrateParam, "GetGyroscopeLowCalibrateParam"},
        {0x00170000, &Spvr::GetSoundVolume, "GetSoundVolume"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::HID
