// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/hle/hle.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cam/cam_u.h"

namespace Service {
namespace CAM {

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, nullptr,                             "StartCapture"},
    {0x00020040, nullptr,                             "StopCapture"},
    {0x00030040, nullptr,                             "IsBusy"},
    {0x00040040, nullptr,                             "ClearBuffer"},
    {0x00050040, nullptr,                             "GetVsyncInterruptEvent"},
    {0x00060040, nullptr,                             "GetBufferErrorInterruptEvent"},
    {0x00070102, nullptr,                             "SetReceiving"},
    {0x00080040, nullptr,                             "IsFinishedReceiving"},
    {0x00090100, nullptr,                             "SetTransferLines"},
    {0x000A0080, nullptr,                             "GetMaxLines"},
    {0x000B0100, nullptr,                             "SetTransferBytes"},
    {0x000C0040, nullptr,                             "GetTransferBytes"},
    {0x000D0080, nullptr,                             "GetMaxBytes"},
    {0x000E0080, nullptr,                             "SetTrimming"},
    {0x000F0040, nullptr,                             "IsTrimming"},
    {0x00100140, nullptr,                             "SetTrimmingParams"},
    {0x00110040, nullptr,                             "GetTrimmingParams"},
    {0x00120140, nullptr,                             "SetTrimmingParamsCenter"},
    {0x00130040, nullptr,                             "Activate"},
    {0x00140080, nullptr,                             "SwitchContext"},
    {0x00150080, nullptr,                             "SetExposure"},
    {0x00160080, nullptr,                             "SetWhiteBalance"},
    {0x00170080, nullptr,                             "SetWhiteBalanceWithoutBaseUp"},
    {0x00180080, nullptr,                             "SetSharpness"},
    {0x00190080, nullptr,                             "SetAutoExposure"},
    {0x001A0040, nullptr,                             "IsAutoExposure"},
    {0x001B0080, nullptr,                             "SetAutoWhiteBalance"},
    {0x001C0040, nullptr,                             "IsAutoWhiteBalance"},
    {0x001D00C0, nullptr,                             "FlipImage"},
    {0x001E0200, nullptr,                             "SetDetailSize"},
    {0x001F00C0, nullptr,                             "SetSize"},
    {0x00200080, nullptr,                             "SetFrameRate"},
    {0x00210080, nullptr,                             "SetPhotoMode"},
    {0x002200C0, nullptr,                             "SetEffect"},
    {0x00230080, nullptr,                             "SetContrast"},
    {0x00240080, nullptr,                             "SetLensCorrection"},
    {0x002500C0, nullptr,                             "SetOutputFormat"},
    {0x00260140, nullptr,                             "SetAutoExposureWindow"},
    {0x00270140, nullptr,                             "SetAutoWhiteBalanceWindow"},
    {0x00280080, nullptr,                             "SetNoiseFilter"},
    {0x00290080, nullptr,                             "SynchronizeVsyncTiming"},
    {0x002A0080, nullptr,                             "GetLatestVsyncTiming"},
    {0x002B0000, nullptr,                             "GetStereoCameraCalibrationData"},
    {0x002C0400, nullptr,                             "SetStereoCameraCalibrationData"},
    {0x00310180, nullptr,                             "SetImageQualityCalibrationData"},
    {0x00320000, nullptr,                             "GetImageQualityCalibrationData"},
    {0x003302C0, nullptr,                             "SetPackageParameterWithoutContext"},
    {0x00340140, nullptr,                             "SetPackageParameterWithContext"},
    {0x003501C0, nullptr,                             "SetPackageParameterWithContextDetail"},
    {0x00360000, nullptr,                             "GetSuitableY2rStandardCoefficient"},
    {0x00380040, nullptr,                             "PlayShutterSound"},
    {0x00390000, nullptr,                             "DriverInitialize"},
    {0x003A0000, nullptr,                             "DriverFinalize"},
    {0x003B0000, nullptr,                             "GetActivatedCamera"},
    {0x003C0000, nullptr,                             "GetSleepCamera"},
    {0x003D0040, nullptr,                             "SetSleepCamera"},
    {0x003E0040, nullptr,                             "SetBrightnessSynchronization"},
};

CAM_U_Interface::CAM_U_Interface() {
    Register(FunctionTable);
}

} // namespace CAM
} // namespace Service
