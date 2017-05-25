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

enum CameraIndex {
    OuterRightCamera = 0,
    InnerCamera = 1,
    OuterLeftCamera = 2,

    NumCameras = 3,
};

enum class Effect : u8 {
    None = 0,
    Mono = 1,
    Sepia = 2,
    Negative = 3,
    Negafilm = 4,
    Sepia01 = 5,
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

/**
 * Resolution parameters for the camera.
 * The native resolution of 3DS camera is 640 * 480. The captured image will be cropped in the
 * region [crop_x0, crop_x1] * [crop_y0, crop_y1], and then scaled to size width * height as the
 * output image. Note that all cropping coordinates are inclusive.
 */
struct Resolution {
    u16 width;
    u16 height;
    u16 crop_x0;
    u16 crop_y0;
    u16 crop_x1;
    u16 crop_y1;
};

struct PackageParameterWithoutContext {
    u8 camera_select;
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
    INSERT_PADDING_WORDS(4);
};

static_assert(sizeof(PackageParameterWithoutContext) == 44,
              "PackageParameterCameraWithoutContext structure size is wrong");

struct PackageParameterWithContext {
    u8 camera_select;
    u8 context_select;
    Flip flip;
    Effect effect;
    Size size;
    INSERT_PADDING_BYTES(3);
    INSERT_PADDING_WORDS(3);

    Resolution GetResolution() const;
};

static_assert(sizeof(PackageParameterWithContext) == 20,
              "PackageParameterWithContext structure size is wrong");

struct PackageParameterWithContextDetail {
    u8 camera_select;
    u8 context_select;
    Flip flip;
    Effect effect;
    Resolution resolution;
    INSERT_PADDING_WORDS(3);

    Resolution GetResolution() const {
        return resolution;
    }
};

static_assert(sizeof(PackageParameterWithContextDetail) == 28,
              "PackageParameterWithContextDetail structure size is wrong");

/**
 * Starts capturing at the selected port.
 *  Inputs:
 *      0: 0x00010040
 *      1: u8 selected port
 *  Outputs:
 *      0: 0x00010040
 *      1: ResultCode
 */
void StartCapture(Service::Interface* self);

/**
 * Stops capturing from the selected port.
 *  Inputs:
 *      0: 0x00020040
 *      1: u8 selected port
 *  Outputs:
 *      0: 0x00020040
 *      1: ResultCode
 */
void StopCapture(Service::Interface* self);

/**
 * Gets whether the selected port is currently capturing.
 *  Inputs:
 *      0: 0x00030040
 *      1: u8 selected port
 *  Outputs:
 *      0: 0x00030080
 *      1: ResultCode
 *      2: 0 if not capturing, 1 if capturing
 */
void IsBusy(Service::Interface* self);

/**
 * Clears the buffer of selected ports.
 *  Inputs:
 *      0: 0x00040040
 *      1: u8 selected port
 *  Outputs:
 *      0: 0x00040040
 *      2: ResultCode
 */
void ClearBuffer(Service::Interface* self);

/**
 * Unknown
 *  Inputs:
 *      0: 0x00050040
 *      1: u8 selected port
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
 *      1: u8 selected port
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
 *      2: u8 selected port
 *      3: Image size (in bytes)
 *      4: u16 Transfer unit size (in bytes)
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
 * Gets whether the selected port finished receiving a frame.
 *  Inputs:
 *      0: 0x00080040
 *      1: u8 selected port
 *  Outputs:
 *      0: 0x00080080
 *      1: ResultCode
 *      2: 0 if not finished, 1 if finished
 */
void IsFinishedReceiving(Service::Interface* self);

/**
 * Sets the number of lines the buffer contains.
 *  Inputs:
 *      0: 0x00090100
 *      1: u8 selected port
 *      2: u16 Number of lines to transfer
 *      3: u16 Width
 *      4: u16 Height
 *  Outputs:
 *      0: 0x00090040
 *      1: ResultCode
 * @todo figure out how the "buffer" actually works.
 */
void SetTransferLines(Service::Interface* self);

/**
 * Gets the maximum number of lines that fit in the buffer
 *  Inputs:
 *      0: 0x000A0080
 *      1: u16 Width
 *      2: u16 Height
 *  Outputs:
 *      0: 0x000A0080
 *      1: ResultCode
 *      2: Maximum number of lines that fit in the buffer
 * @todo figure out how the "buffer" actually works.
 */
void GetMaxLines(Service::Interface* self);

/**
 * Sets the number of bytes the buffer contains.
 *  Inputs:
 *      0: 0x000B0100
 *      1: u8 selected port
 *      2: u16 Number of bytes to transfer
 *      3: u16 Width
 *      4: u16 Height
 *  Outputs:
 *      0: 0x000B0040
 *      1: ResultCode
 * @todo figure out how the "buffer" actually works.
 */
void SetTransferBytes(Service::Interface* self);

/**
 * Gets the number of bytes to the buffer contains.
 *  Inputs:
 *      0: 0x000C0040
 *      1: u8 selected port
 *  Outputs:
 *      0: 0x000C0080
 *      1: ResultCode
 *      2: The number of bytes the buffer contains
 * @todo figure out how the "buffer" actually works.
 */
void GetTransferBytes(Service::Interface* self);

/**
 * Gets the maximum number of bytes that fit in the buffer.
 *  Inputs:
 *      0: 0x000D0080
 *      1: u16 Width
 *      2: u16 Height
 *  Outputs:
 *      0: 0x000D0080
 *      1: ResultCode
 *      2: Maximum number of bytes that fit in the buffer
 * @todo figure out how the "buffer" actually works.
 */
void GetMaxBytes(Service::Interface* self);

/**
 * Enables or disables trimming.
 *  Inputs:
 *      0: 0x000E0080
 *      1: u8 selected port
 *      2: u8 bool Enable trimming if true
 *  Outputs:
 *      0: 0x000E0040
 *      1: ResultCode
 */
void SetTrimming(Service::Interface* self);

/**
 * Gets whether trimming is enabled.
 *  Inputs:
 *      0: 0x000F0040
 *      1: u8 selected port
 *  Outputs:
 *      0: 0x000F0080
 *      1: ResultCode
 *      2: u8 bool Enable trimming if true
 */
void IsTrimming(Service::Interface* self);

/**
 * Sets the position to trim.
 *  Inputs:
 *      0: 0x00100140
 *      1: u8 selected port
 *      2: x start
 *      3: y start
 *      4: x end (exclusive)
 *      5: y end (exclusive)
 *  Outputs:
 *      0: 0x00100040
 *      1: ResultCode
 */
void SetTrimmingParams(Service::Interface* self);

/**
 * Gets the position to trim.
 *  Inputs:
 *      0: 0x00110040
 *      1: u8 selected port
 *
 *  Outputs:
 *      0: 0x00110140
 *      1: ResultCode
 *      2: x start
 *      3: y start
 *      4: x end (exclusive)
 *      5: y end (exclusive)
 */
void GetTrimmingParams(Service::Interface* self);

/**
 * Sets the position to trim by giving the width and height. The trimming window is always at the
 * center.
 *  Inputs:
 *      0: 0x00120140
 *      1: u8 selected port
 *      2: s16 Trim width
 *      3: s16 Trim height
 *      4: s16 Camera width
 *      5: s16 Camera height
 *  Outputs:
 *      0: 0x00120040
 *      1: ResultCode
 */
void SetTrimmingParamsCenter(Service::Interface* self);

/**
 * Selects up to two physical cameras to enable.
 *  Inputs:
 *      0: 0x00130040
 *      1: u8 selected camera
 *  Outputs:
 *      0: 0x00130040
 *      1: ResultCode
 */
void Activate(Service::Interface* self);

/**
 * Switches the context of camera settings.
 *  Inputs:
 *      0: 0x00140080
 *      1: u8 selected camera
 *      2: u8 selected context
 *  Outputs:
 *      0: 0x00140040
 *      1: ResultCode
 */
void SwitchContext(Service::Interface* self);

/**
 * Sets flipping of images
 *  Inputs:
 *      0: 0x001D00C0
 *      1: u8 selected camera
 *      2: u8 Type of flipping to perform (`Flip` enum)
 *      3: u8 selected context
 *  Outputs:
 *      0: 0x001D0040
 *      1: ResultCode
 */
void FlipImage(Service::Interface* self);

/**
 * Sets camera resolution from custom parameters. For more details see the Resolution struct.
 *  Inputs:
 *      0: 0x001E0200
 *      1: u8 selected camera
 *      2: width
 *      3: height
 *      4: crop x0
 *      5: crop y0
 *      6: crop x1
 *      7: crop y1
 *      8: u8 selected context
 *  Outputs:
 *      0: 0x001E0040
 *      1: ResultCode
 */
void SetDetailSize(Service::Interface* self);

/**
 * Sets camera resolution from preset resolution parameters.
 *  Inputs:
 *      0: 0x001F00C0
 *      1: u8 selected camera
 *      2: u8 Camera frame resolution (`Size` enum)
 *      3: u8 selected context
 *  Outputs:
 *      0: 0x001F0040
 *      1: ResultCode
 */
void SetSize(Service::Interface* self);

/**
 * Sets camera framerate.
 *  Inputs:
 *      0: 0x00200080
 *      1: u8 selected camera
 *      2: u8 Camera framerate (`FrameRate` enum)
 *  Outputs:
 *      0: 0x00200040
 *      1: ResultCode
 */
void SetFrameRate(Service::Interface* self);

/**
 * Sets effect on the output image
 *  Inputs:
 *      0: 0x002200C0
 *      1: u8 selected camera
 *      2: u8 image effect (`Effect` enum)
 *      3: u8 selected context
 *  Outputs:
 *      0: 0x00220040
 *      1: ResultCode
 */
void SetEffect(Service::Interface* self);

/**
 * Sets format of the output image
 *  Inputs:
 *      0: 0x002500C0
 *      1: u8 selected camera
 *      2: u8 image format (`OutputFormat` enum)
 *      3: u8 selected context
 *  Outputs:
 *      0: 0x00250040
 *      1: ResultCode
 */
void SetOutputFormat(Service::Interface* self);

/**
 * Synchronizes the V-Sync timing of two cameras.
 *  Inputs:
 *      0: 0x00290080
 *      1: u8 selected camera 1
 *      2: u8 selected camera 2
 *  Outputs:
 *      0: 0x00280040
 *      1: ResultCode
 */
void SynchronizeVsyncTiming(Service::Interface* self);

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
 * Batch-configures context-free settings.
 *
 *  Inputs:
 *      0: 0x003302C0
 *      1-7: struct PachageParameterWithoutContext
 *      8-11: unused
 *  Outputs:
 *      0: 0x00330040
 *      1: ResultCode
 */
void SetPackageParameterWithoutContext(Service::Interface* self);

/**
 * Batch-configures context-related settings with preset resolution parameters.
 *
 *  Inputs:
 *      0: 0x00340140
 *      1-2: struct PackageParameterWithContext
 *      3-5: unused
 *  Outputs:
 *      0: 0x00340040
 *      1: ResultCode
 */
void SetPackageParameterWithContext(Service::Interface* self);

/**
 * Batch-configures context-related settings with custom resolution parameters
 *
 *  Inputs:
 *      0: 0x003501C0
 *      1-4: struct PackageParameterWithContextDetail
 *      5-7: unused
 *  Outputs:
 *      0: 0x00350040
 *      1: ResultCode
 */
void SetPackageParameterWithContextDetail(Service::Interface* self);

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
