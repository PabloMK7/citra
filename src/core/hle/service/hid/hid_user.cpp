// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/hid/hid_user.h"

SERIALIZE_EXPORT_IMPL(Service::HID::User)

namespace Service::HID {

User::User(std::shared_ptr<Module> hid) : Module::Interface(std::move(hid), "hid:USER", 6) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 8, 0), nullptr, "CalibrateTouchScreen"},
        {IPC::MakeHeader(0x0002, 0, 0), nullptr, "UpdateTouchConfig"},
        {IPC::MakeHeader(0x000A, 0, 0), &User::GetIPCHandles, "GetIPCHandles"},
        {IPC::MakeHeader(0x000B, 0, 0), nullptr, "StartAnalogStickCalibration"},
        {IPC::MakeHeader(0x000E, 0, 0), nullptr, "GetAnalogStickCalibrateParam"},
        {IPC::MakeHeader(0x0011, 0, 0), &User::EnableAccelerometer, "EnableAccelerometer"},
        {IPC::MakeHeader(0x0012, 0, 0), &User::DisableAccelerometer, "DisableAccelerometer"},
        {IPC::MakeHeader(0x0013, 0, 0), &User::EnableGyroscopeLow, "EnableGyroscopeLow"},
        {IPC::MakeHeader(0x0014, 0, 0), &User::DisableGyroscopeLow, "DisableGyroscopeLow"},
        {IPC::MakeHeader(0x0015, 0, 0), &User::GetGyroscopeLowRawToDpsCoefficient, "GetGyroscopeLowRawToDpsCoefficient"},
        {IPC::MakeHeader(0x0016, 0, 0), &User::GetGyroscopeLowCalibrateParam, "GetGyroscopeLowCalibrateParam"},
        {IPC::MakeHeader(0x0017, 0, 0), &User::GetSoundVolume, "GetSoundVolume"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::HID
