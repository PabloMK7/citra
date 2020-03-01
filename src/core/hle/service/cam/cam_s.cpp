// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cam/cam_s.h"

namespace Service::CAM {

CAM_S::CAM_S(std::shared_ptr<Module> cam) : Module::Interface(std::move(cam), "cam:s", 1) {
    static const FunctionInfo functions[] = {
        {0x00010040, &CAM_S::StartCapture, "StartCapture"},
        {0x00020040, &CAM_S::StopCapture, "StopCapture"},
        {0x00030040, &CAM_S::IsBusy, "IsBusy"},
        {0x00040040, &CAM_S::ClearBuffer, "ClearBuffer"},
        {0x00050040, &CAM_S::GetVsyncInterruptEvent, "GetVsyncInterruptEvent"},
        {0x00060040, &CAM_S::GetBufferErrorInterruptEvent, "GetBufferErrorInterruptEvent"},
        {0x00070102, &CAM_S::SetReceiving, "SetReceiving"},
        {0x00080040, &CAM_S::IsFinishedReceiving, "IsFinishedReceiving"},
        {0x00090100, &CAM_S::SetTransferLines, "SetTransferLines"},
        {0x000A0080, &CAM_S::GetMaxLines, "GetMaxLines"},
        {0x000B0100, &CAM_S::SetTransferBytes, "SetTransferBytes"},
        {0x000C0040, &CAM_S::GetTransferBytes, "GetTransferBytes"},
        {0x000D0080, &CAM_S::GetMaxBytes, "GetMaxBytes"},
        {0x000E0080, &CAM_S::SetTrimming, "SetTrimming"},
        {0x000F0040, &CAM_S::IsTrimming, "IsTrimming"},
        {0x00100140, &CAM_S::SetTrimmingParams, "SetTrimmingParams"},
        {0x00110040, &CAM_S::GetTrimmingParams, "GetTrimmingParams"},
        {0x00120140, &CAM_S::SetTrimmingParamsCenter, "SetTrimmingParamsCenter"},
        {0x00130040, &CAM_S::Activate, "Activate"},
        {0x00140080, &CAM_S::SwitchContext, "SwitchContext"},
        {0x00150080, nullptr, "SetExposure"},
        {0x00160080, nullptr, "SetWhiteBalance"},
        {0x00170080, nullptr, "SetWhiteBalanceWithoutBaseUp"},
        {0x00180080, nullptr, "SetSharpness"},
        {0x00190080, nullptr, "SetAutoExposure"},
        {0x001A0040, nullptr, "IsAutoExposure"},
        {0x001B0080, nullptr, "SetAutoWhiteBalance"},
        {0x001C0040, nullptr, "IsAutoWhiteBalance"},
        {0x001D00C0, &CAM_S::FlipImage, "FlipImage"},
        {0x001E0200, &CAM_S::SetDetailSize, "SetDetailSize"},
        {0x001F00C0, &CAM_S::SetSize, "SetSize"},
        {0x00200080, &CAM_S::SetFrameRate, "SetFrameRate"},
        {0x00210080, nullptr, "SetPhotoMode"},
        {0x002200C0, &CAM_S::SetEffect, "SetEffect"},
        {0x00230080, nullptr, "SetContrast"},
        {0x00240080, nullptr, "SetLensCorrection"},
        {0x002500C0, &CAM_S::SetOutputFormat, "SetOutputFormat"},
        {0x00260140, nullptr, "SetAutoExposureWindow"},
        {0x00270140, nullptr, "SetAutoWhiteBalanceWindow"},
        {0x00280080, nullptr, "SetNoiseFilter"},
        {0x00290080, &CAM_S::SynchronizeVsyncTiming, "SynchronizeVsyncTiming"},
        {0x002A0080, &CAM_S::GetLatestVsyncTiming, "GetLatestVsyncTiming"},
        {0x002B0000, &CAM_S::GetStereoCameraCalibrationData, "GetStereoCameraCalibrationData"},
        {0x002C0400, nullptr, "SetStereoCameraCalibrationData"},
        {0x002D00C0, nullptr, "WriteRegisterI2c"},
        {0x002E00C0, nullptr, "WriteMcuVariableI2c"},
        {0x002F0080, nullptr, "ReadRegisterI2cExclusive"},
        {0x00300080, nullptr, "ReadMcuVariableI2cExclusive"},
        {0x00310180, nullptr, "SetImageQualityCalibrationData"},
        {0x00320000, nullptr, "GetImageQualityCalibrationData"},
        {0x003302C0, &CAM_S::SetPackageParameterWithoutContext,
         "SetPackageParameterWithoutContext"},
        {0x00340140, &CAM_S::SetPackageParameterWithContext, "SetPackageParameterWithContext"},
        {0x003501C0, &CAM_S::SetPackageParameterWithContextDetail,
         "SetPackageParameterWithContextDetail"},
        {0x00360000, &CAM_S::GetSuitableY2rStandardCoefficient,
         "GetSuitableY2rStandardCoefficient"},
        {0x00370202, nullptr, "PlayShutterSoundWithWave"},
        {0x00380040, &CAM_S::PlayShutterSound, "PlayShutterSound"},
        {0x00390000, &CAM_S::DriverInitialize, "DriverInitialize"},
        {0x003A0000, &CAM_S::DriverFinalize, "DriverFinalize"},
        {0x003B0000, nullptr, "GetActivatedCamera"},
        {0x003C0000, nullptr, "GetSleepCamera"},
        {0x003D0040, nullptr, "SetSleepCamera"},
        {0x003E0040, nullptr, "SetBrightnessSynchronization"},
    };
    RegisterHandlers(functions);
}

} // namespace Service::CAM
