// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include "common/common_types.h"
#include "core/hle/result.h"
#include "core/hle/service/service.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Y2R_U

namespace Y2R_U {

enum class InputFormat : u8 {
    /// 8-bit input, with YUV components in separate planes and 4:2:2 subsampling.
    YUV422_Indiv8 = 0,
    /// 8-bit input, with YUV components in separate planes and 4:2:0 subsampling.
    YUV420_Indiv8 = 1,

    /// 16-bit input (only LSB used), with YUV components in separate planes and 4:2:2 subsampling.
    YUV422_Indiv16 = 2,
    /// 16-bit input (only LSB used), with YUV components in separate planes and 4:2:0 subsampling.
    YUV420_Indiv16 = 3,

    /// 8-bit input, with a single interleaved stream in YUYV format and 4:2:2 subsampling.
    YUYV422_Interleaved = 4,
};

enum class OutputFormat : u8 {
    RGBA8 = 0,
    RGB8 = 1,
    RGB5A1 = 2,
    RGB565 = 3,
};

enum class Rotation : u8 {
    None = 0,
    Clockwise_90 = 1,
    Clockwise_180 = 2,
    Clockwise_270 = 3,
};

enum class BlockAlignment : u8 {
    /// Image is output in linear format suitable for use as a framebuffer.
    Linear = 0,
    /// Image is output in tiled PICA format, suitable for use as a texture.
    Block8x8 = 1,
};

enum class StandardCoefficient : u8 {
    /// ITU Rec. BT.601 primaries, with PC ranges.
    ITU_Rec601 = 0,
    /// ITU Rec. BT.709 primaries, with PC ranges.
    ITU_Rec709 = 1,
    /// ITU Rec. BT.601 primaries, with TV ranges.
    ITU_Rec601_Scaling = 2,
    /// ITU Rec. BT.709 primaries, with TV ranges.
    ITU_Rec709_Scaling = 3,
};

/**
 * A set of coefficients configuring the RGB to YUV conversion. Coefficients 0-4 are unsigned 2.8
 * fixed pointer numbers representing entries on the conversion matrix, while coefficient 5-7 are
 * signed 11.5 fixed point numbers added as offsets to the RGB result.
 *
 * The overall conversion process formula is:
 * ```
 * R = trunc((c_0 * Y           + c_1 * V) + c_5 + 0.75)
 * G = trunc((c_0 * Y - c_3 * U - c_2 * V) + c_6 + 0.75)
 * B = trunc((c_0 * Y + c_4 * U          ) + c_7 + 0.75)
 * ```
 */
using CoefficientSet = std::array<s16, 8>;

struct ConversionBuffer {
    /// Current reading/writing address of this buffer.
    VAddr address;
    /// Remaining amount of bytes to be DMAed, does not include the inter-trasfer gap.
    u32 image_size;
    /// Size of a single DMA transfer.
    u16 transfer_unit;
    /// Amount of bytes to be skipped between copying each `transfer_unit` bytes.
    u16 gap;
};

struct ConversionConfiguration {
    InputFormat input_format;
    OutputFormat output_format;
    Rotation rotation;
    BlockAlignment block_alignment;
    u16 input_line_width;
    u16 input_lines;
    CoefficientSet coefficients;
    u8 padding;
    u16 alpha;

    /// Input parameters for the Y (luma) plane
    ConversionBuffer src_Y, src_U, src_V, src_YUYV;
    /// Output parameters for the conversion results
    ConversionBuffer dst;

    ResultCode SetInputLineWidth(u16 width);
    ResultCode SetInputLines(u16 lines);
    ResultCode SetStandardCoefficient(StandardCoefficient standard_coefficient);
};

struct DitheringWeightParams {
    u16 w0_xEven_yEven;
    u16 w0_xOdd_yEven;
    u16 w0_xEven_yOdd;
    u16 w0_xOdd_yOdd;
    u16 w1_xEven_yEven;
    u16 w1_xOdd_yEven;
    u16 w1_xEven_yOdd;
    u16 w1_xOdd_yOdd;
    u16 w2_xEven_yEven;
    u16 w2_xOdd_yEven;
    u16 w2_xEven_yOdd;
    u16 w2_xOdd_yOdd;
    u16 w3_xEven_yEven;
    u16 w3_xOdd_yEven;
    u16 w3_xEven_yOdd;
    u16 w3_xOdd_yOdd;
};

class Interface : public Service::Interface {
public:
    Interface();
    ~Interface() override;

    std::string GetPortName() const override {
        return "y2r:u";
    }
};

} // namespace
