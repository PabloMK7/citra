// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cam/cam_c.h"

namespace Service::CAM {

CAM_C::CAM_C(std::shared_ptr<Module> cam) : Module::Interface(std::move(cam), "cam:c", 1) {
    static const FunctionInfo functions[] = {
        {0x00010040, &CAM_C::StartCapture, "StartCapture"},
        {0x00020040, &CAM_C::StopCapture, "StopCapture"},
        {0x00030040, &CAM_C::IsBusy, "IsBusy"},
        {0x00040040, &CAM_C::ClearBuffer, "ClearBuffer"},
        {0x00050040, &CAM_C::GetVsyncInterruptEvent, "GetVsyncInterruptEvent"},
        {0x00060040, &CAM_C::GetBufferErrorInterruptEvent, "GetBufferErrorInterruptEvent"},
        {0x00070102, &CAM_C::SetReceiving, "SetReceiving"},
        {0x00080040, &CAM_C::IsFinishedReceiving, "IsFinishedReceiving"},
        {0x00090100, &CAM_C::SetTransferLines, "SetTransferLines"},
        {0x000A0080, &CAM_C::GetMaxLines, "GetMaxLines"},
        {0x000B0100, &CAM_C::SetTransferBytes, "SetTransferBytes"},
        {0x000C0040, &CAM_C::GetTransferBytes, "GetTransferBytes"},
        {0x000D0080, &CAM_C::GetMaxBytes, "GetMaxBytes"},
        {0x000E0080, &CAM_C::SetTrimming, "SetTrimming"},
        {0x000F0040, &CAM_C::IsTrimming, "IsTrimming"},
        {0x00100140, &CAM_C::SetTrimmingParams, "SetTrimmingParams"},
        {0x00110040, &CAM_C::GetTrimmingParams, "GetTrimmingParams"},
        {0x00120140, &CAM_C::SetTrimmingParamsCenter, "SetTrimmingParamsCenter"},
        {0x00130040, &CAM_C::Activate, "Activate"},
        {0x00140080, &CAM_C::SwitchContext, "SwitchContext"},
        {0x00150080, nullptr, "SetExposure"},
        {0x00160080, nullptr, "SetWhiteBalance"},
        {0x00170080, nullptr, "SetWhiteBalanceWithoutBaseUp"},
        {0x00180080, nullptr, "SetSharpness"},
        {0x00190080, nullptr, "SetAutoExposure"},
        {0x001A0040, nullptr, "IsAutoExposure"},
        {0x001B0080, nullptr, "SetAutoWhiteBalance"},
        {0x001C0040, nullptr, "IsAutoWhiteBalance"},
        {0x001D00C0, &CAM_C::FlipImage, "FlipImage"},
        {0x001E0200, &CAM_C::SetDetailSize, "SetDetailSize"},
        {0x001F00C0, &CAM_C::SetSize, "SetSize"},
        {0x00200080, &CAM_C::SetFrameRate, "SetFrameRate"},
        {0x00210080, nullptr, "SetPhotoMode"},
        {0x002200C0, &CAM_C::SetEffect, "SetEffect"},
        {0x00230080, nullptr, "SetContrast"},
        {0x00240080, nullptr, "SetLensCorrection"},
        {0x002500C0, &CAM_C::SetOutputFormat, "SetOutputFormat"},
        {0x00260140, nullptr, "SetAutoExposureWindow"},
        {0x00270140, nullptr, "SetAutoWhiteBalanceWindow"},
        {0x00280080, nullptr, "SetNoiseFilter"},
        {0x00290080, &CAM_C::SynchronizeVsyncTiming, "SynchronizeVsyncTiming"},
        {0x002A0080, &CAM_C::GetLatestVsyncTiming, "GetLatestVsyncTiming"},
        {0x002B0000, &CAM_C::GetStereoCameraCalibrationData, "GetStereoCameraCalibrationData"},
        {0x002C0400, nullptr, "SetStereoCameraCalibrationData"},
        {0x002D00C0, nullptr, "WriteRegisterI2c"},
        {0x002E00C0, nullptr, "WriteMcuVariableI2c"},
        {0x002F0080, nullptr, "ReadRegisterI2cExclusive"},
        {0x00300080, nullptr, "ReadMcuVariableI2cExclusive"},
        {0x00310180, nullptr, "SetImageQualityCalibrationData"},
        {0x00320000, nullptr, "GetImageQualityCalibrationData"},
        {0x003302C0, &CAM_C::SetPackageParameterWithoutContext,
         "SetPackageParameterWithoutContext"},
        {0x00340140, &CAM_C::SetPackageParameterWithContext, "SetPackageParameterWithContext"},
        {0x003501C0, &CAM_C::SetPackageParameterWithContextDetail,
         "SetPackageParameterWithContextDetail"},
        {0x00360000, &CAM_C::GetSuitableY2rStandardCoefficient,
         "GetSuitableY2rStandardCoefficient"},
        {0x00370202, nullptr, "PlayShutterSoundWithWave"},
        {0x00380040, &CAM_C::PlayShutterSound, "PlayShutterSound"},
        {0x00390000, &CAM_C::DriverInitialize, "DriverInitialize"},
        {0x003A0000, &CAM_C::DriverFinalize, "DriverFinalize"},
        {0x003B0000, nullptr, "GetActivatedCamera"},
        {0x003C0000, nullptr, "GetSleepCamera"},
        {0x003D0040, nullptr, "SetSleepCamera"},
        {0x003E0040, nullptr, "SetBrightnessSynchronization"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::CAM
