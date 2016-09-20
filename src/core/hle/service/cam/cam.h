// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/swap.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/service.h"

namespace Service {
namespace CAM {

enum class Port : u8 { None = 0, Cam1 = 1, Cam2 = 2, Both = Cam1 | Cam2 };

enum class CameraSelect : u8 {
    None = 0,
    Out1 = 1,
    In1 = 2,
    Out2 = 4,
    In1Out1 = Out1 | In1,
    Out1Out2 = Out1 | Out2,
    In1Out2 = In1 | Out2,
    All = Out1 | In1 | Out2,
};

enum class Effect : u8 {
    None = 0,
    Mono = 1,
    Sepia = 2,
    Negative = 3,
    Negafilm = 4,
    Sepia01 = 5,
};

enum class Context : u8 {
    None = 0,
    A = 1,
    B = 2,
    Both = A | B,
};

enum class Flip : u8 {
    None = 0,
    Horizontal = 1,
    Vertical = 2,
    Reverse = 3,
};

enum class Size : u8 {
    VGA = 0,
    QVGA = 1,
    QQVGA = 2,
    CIF = 3,
    QCIF = 4,
    DS_LCD = 5,
    DS_LCDx4 = 6,
    CTR_TOP_LCD = 7,
    CTR_BOTTOM_LCD = QVGA,
};

enum class FrameRate : u8 {
    Rate_15 = 0,
    Rate_15_To_5 = 1,
    Rate_15_To_2 = 2,
    Rate_10 = 3,
    Rate_8_5 = 4,
    Rate_5 = 5,
    Rate_20 = 6,
    Rate_20_To_5 = 7,
    Rate_30 = 8,
    Rate_30_To_5 = 9,
    Rate_15_To_10 = 10,
    Rate_20_To_10 = 11,
    Rate_30_To_10 = 12,
};

enum class ShutterSoundType : u8 {
    Normal = 0,
    Movie = 1,
    MovieEnd = 2,
};

enum class WhiteBalance : u8 {
    BalanceAuto = 0,
    Balance3200K = 1,
    Balance4150K = 2,
    Balance5200K = 3,
    Balance6000K = 4,
    Balance7000K = 5,
    BalanceMax = 6,
    BalanceNormal = BalanceAuto,
    BalanceTungsten = Balance3200K,
    BalanceWhiteFluorescentLight = Balance4150K,
    BalanceDaylight = Balance5200K,
    BalanceCloudy = Balance6000K,
    BalanceHorizon = Balance6000K,
    BalanceShade = Balance7000K,
};

enum class PhotoMode : u8 {
    Normal = 0,
    Portrait = 1,
    Landscape = 2,
    Nightview = 3,
    Letter0 = 4,
};

enum class LensCorrection : u8 {
    Off = 0,
    On70 = 1,
    On90 = 2,
    Dark = Off,
    Normal = On70,
    Bright = On90,
};

enum class Contrast : u8 {
    Pattern01 = 1,
    Pattern02 = 2,
    Pattern03 = 3,
    Pattern04 = 4,
    Pattern05 = 5,
    Pattern06 = 6,
    Pattern07 = 7,
    Pattern08 = 8,
    Pattern09 = 9,
    Pattern10 = 10,
    Pattern11 = 11,
    Low = Pattern05,
    Normal = Pattern06,
    High = Pattern07,
};

enum class OutputFormat : u8 {
    YUV422 = 0,
    RGB565 = 1,
};

/// Stereo camera calibration data.
struct StereoCameraCalibrationData {
    u8 isValidRotationXY; ///< Bool indicating whether the X and Y rotation data is valid.
    INSERT_PADDING_BYTES(3);
    float_le scale;        ///< Scale to match the left camera image with the right.
    float_le rotationZ;    ///< Z axis rotation to match the left camera image with the right.
    float_le translationX; ///< X axis translation to match the left camera image with the right.
    float_le translationY; ///< Y axis translation to match the left camera image with the right.
    float_le rotationX;    ///< X axis rotation to match the left camera image with the right.
    float_le rotationY;    ///< Y axis rotation to match the left camera image with the right.
    float_le angleOfViewRight; ///< Right camera angle of view.
    float_le angleOfViewLeft;  ///< Left camera angle of view.
    float_le distanceToChart;  ///< Distance between cameras and measurement chart.
    float_le distanceCameras;  ///< Distance between left and right cameras.
    s16_le imageWidth;         ///< Image width.
    s16_le imageHeight;        ///< Image height.
    INSERT_PADDING_BYTES(16);
};
static_assert(sizeof(StereoCameraCalibrationData) == 64,
              "StereoCameraCalibrationData structure size is wrong");

struct PackageParameterCameraSelect {
    CameraSelect camera;
    s8 exposure;
    WhiteBalance white_balance;
    s8 sharpness;
    bool auto_exposure;
    bool auto_white_balance;
    FrameRate frame_rate;
    PhotoMode photo_mode;
    Contrast contrast;
    LensCorrection lens_correction;
    bool noise_filter;
    u8 padding;
    s16 auto_exposure_window_x;
    s16 auto_exposure_window_y;
    s16 auto_exposure_window_width;
    s16 auto_exposure_window_height;
    s16 auto_white_balance_window_x;
    s16 auto_white_balance_window_y;
    s16 auto_white_balance_window_width;
    s16 auto_white_balance_window_height;
};

static_assert(sizeof(PackageParameterCameraSelect) == 28,
              "PackageParameterCameraSelect structure size is wrong");

/**
 * Unknown
 *  Inputs:
 *      0: 0x00010040
 *      1: u8 Camera port (`Port` enum)
 *  Outputs:
 *      0: 0x00010040
 *      1: ResultCode
 */
void StartCapture(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x00020040
 *      1: u8 Camera port (`Port` enum)
 *  Outputs:
 *      0: 0x00020040
 *      1: ResultCode
 */
void StopCapture(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x00050040
 *      1: u8 Camera port (`Port` enum)
 *  Outputs:
 *      0: 0x00050042
 *      1: ResultCode
 *      2: Descriptor: Handle
 *      3: Event handle
 */
void GetVsyncInterruptEvent(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x00060040
 *      1: u8 Camera port (`Port` enum)
 *  Outputs:
 *      0: 0x00060042
 *      1: ResultCode
 *      2: Descriptor: Handle
 *      3: Event handle
 */
void GetBufferErrorInterruptEvent(Service::Interface* self);

/**
 * Sets the target buffer to receive a frame of image data and starts the transfer. Each camera
 * port has its own event to signal the end of the transfer.
 *
 *  Inputs:
 *      0: 0x00070102
 *      1: Destination address in calling process
 *      2: u8 Camera port (`Port` enum)
 *      3: Image size (in bytes?)
 *      4: u16 Transfer unit size (in bytes?)
 *      5: Descriptor: Handle
 *      6: Handle to destination process
 *  Outputs:
 *      0: 0x00070042
 *      1: ResultCode
 *      2: Descriptor: Handle
 *      3: Handle to event signalled when transfer finishes
 */
void SetReceiving(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x00090100
 *      1: u8 Camera port (`Port` enum)
 *      2: u16 Number of lines to transfer
 *      3: u16 Width
 *      4: u16 Height
 *  Outputs:
 *      0: 0x00090040
 *      1: ResultCode
 */
void SetTransferLines(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x000A0080
 *      1: u16 Width
 *      2: u16 Height
 *  Outputs:
 *      0: 0x000A0080
 *      1: ResultCode
 *      2: Maximum number of lines that fit in the buffer(?)
 */
void GetMaxLines(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x000C0040
 *      1: u8 Camera port (`Port` enum)
 *  Outputs:
 *      0: 0x000C0080
 *      1: ResultCode
 *      2: Total number of bytes for each frame with current settings(?)
 */
void GetTransferBytes(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x000E0080
 *      1: u8 Camera port (`Port` enum)
 *      2: u8 bool Enable trimming if true
 *  Outputs:
 *      0: 0x000E0040
 *      1: ResultCode
 */
void SetTrimming(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x00120140
 *      1: u8 Camera port (`Port` enum)
 *      2: s16 Trim width(?)
 *      3: s16 Trim height(?)
 *      4: s16 Camera width(?)
 *      5: s16 Camera height(?)
 *  Outputs:
 *      0: 0x00120040
 *      1: ResultCode
 */
void SetTrimmingParamsCenter(Service::Interface* self);

/**
 * Selects up to two physical cameras to enable.
 *  Inputs:
 *      0: 0x00130040
 *      1: u8 Cameras to activate (`CameraSelect` enum)
 *  Outputs:
 *      0: 0x00130040
 *      1: ResultCode
 */
void Activate(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x001D00C0
 *      1: u8 Camera select (`CameraSelect` enum)
 *      2: u8 Type of flipping to perform (`Flip` enum)
 *      3: u8 Context (`Context` enum)
 *  Outputs:
 *      0: 0x001D0040
 *      1: ResultCode
 */
void FlipImage(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x001F00C0
 *      1: u8 Camera select (`CameraSelect` enum)
 *      2: u8 Camera frame resolution (`Size` enum)
 *      3: u8 Context id (`Context` enum)
 *  Outputs:
 *      0: 0x001F0040
 *      1: ResultCode
 */
void SetSize(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x00200080
 *      1: u8 Camera select (`CameraSelect` enum)
 *      2: u8 Camera framerate (`FrameRate` enum)
 *  Outputs:
 *      0: 0x00200040
 *      1: ResultCode
 */
void SetFrameRate(Service::Interface* self);

/**
 * Returns calibration data relating the outside cameras to eachother, for use in AR applications.
 *
 *  Inputs:
 *      0: 0x002B0000
 *  Outputs:
 *      0: 0x002B0440
 *      1: ResultCode
 *      2-17: `StereoCameraCalibrationData` structure with calibration values
 */
void GetStereoCameraCalibrationData(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x00360000
 *  Outputs:
 *      0: 0x00360080
 *      1: ResultCode
 *      2: ?
 */
void GetSuitableY2rStandardCoefficient(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x00380040
 *      1: u8 Sound ID
 *  Outputs:
 *      0: 0x00380040
 *      1: ResultCode
 */
void PlayShutterSound(Service::Interface* self);

/**
 * Initializes the camera driver. Must be called before using other functions.
 *  Inputs:
 *      0: 0x00390000
 *  Outputs:
 *      0: 0x00390040
 *      1: ResultCode
 */
void DriverInitialize(Service::Interface* self);

/**
 * Shuts down the camera driver.
 *  Inputs:
 *      0: 0x003A0000
 *  Outputs:
 *      0: 0x003A0040
 *      1: ResultCode
 */
void DriverFinalize(Service::Interface* self);

/// Initialize CAM service(s)
void Init();

/// Shutdown CAM service(s)
void Shutdown();

} // namespace CAM
} // namespace Service
