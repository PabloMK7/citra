// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/archives.h"
#include "core/hle/service/hid/hid_spvr.h"

SERIALIZE_EXPORT_IMPL(Service::HID::Spvr)

namespace Service::HID {

Spvr::Spvr(std::shared_ptr<Module> hid) : Module::Interface(std::move(hid), "hid:SPVR", 6) {
    static const FunctionInfo functions[] = {
        // clang-format off
        {IPC::MakeHeader(0x0001, 8, 0), nullptr, "CalibrateTouchScreen"},
        {IPC::MakeHeader(0x0002, 0, 0), nullptr, "UpdateTouchConfig"},
        {IPC::MakeHeader(0x000A, 0, 0), &Spvr::GetIPCHandles, "GetIPCHandles"},
        {IPC::MakeHeader(0x000B, 0, 0), nullptr, "StartAnalogStickCalibration"},
        {IPC::MakeHeader(0x000E, 0, 0), nullptr, "GetAnalogStickCalibrateParam"},
        {IPC::MakeHeader(0x0011, 0, 0), &Spvr::EnableAccelerometer, "EnableAccelerometer"},
        {IPC::MakeHeader(0x0012, 0, 0), &Spvr::DisableAccelerometer, "DisableAccelerometer"},
        {IPC::MakeHeader(0x0013, 0, 0), &Spvr::EnableGyroscopeLow, "EnableGyroscopeLow"},
        {IPC::MakeHeader(0x0014, 0, 0), &Spvr::DisableGyroscopeLow, "DisableGyroscopeLow"},
        {IPC::MakeHeader(0x0015, 0, 0), &Spvr::GetGyroscopeLowRawToDpsCoefficient, "GetGyroscopeLowRawToDpsCoefficient"},
        {IPC::MakeHeader(0x0016, 0, 0), &Spvr::GetGyroscopeLowCalibrateParam, "GetGyroscopeLowCalibrateParam"},
        {IPC::MakeHeader(0x0017, 0, 0), &Spvr::GetSoundVolume, "GetSoundVolume"},
        // clang-format on
    };
    RegisterHandlers(functions);
}

} // namespace Service::HID
