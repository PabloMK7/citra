// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/logging/log.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/y2r_u.h"
#include "core/mem_map.h"
#include "core/memory.h"

#include "video_core/utils.h"
#include "video_core/video_core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Y2R_U

namespace Y2R_U {

enum class InputFormat : u8 {
    /// 8-bit input, with YUV components in separate planes and using 4:2:2 subsampling.
    YUV422_Indiv8 = 0,
    /// 8-bit input, with YUV components in separate planes and using 4:2:0 subsampling.
    YUV420_Indiv8 = 1,

    YUV422_INDIV_16 = 2,
    YUV420_INDIV_16 = 3,
    YUV422_BATCH = 4,
};

enum class OutputFormat : u8 {
    Rgb32 = 0,
    Rgb24 = 1,
    Rgb16_555 = 2,
    Rgb16_565 = 3,
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
    ITU_Rec601 = 0,
    ITU_Rec709 = 1,
    ITU_Rec601_Scaling = 2,
    ITU_Rec709_Scaling = 3,
};

static Kernel::SharedPtr<Kernel::Event> completion_event;

struct ConversionParameters {
    InputFormat input_format;
    OutputFormat output_format;
    Rotation rotation;
    BlockAlignment block_alignment;
    u16 input_line_width;
    u16 input_lines;
    StandardCoefficient standard_coefficient;
    u8 reserved;
    u16 alpha;
};
static_assert(sizeof(ConversionParameters) == 12, "ConversionParameters struct has incorrect size");

struct ConversionBuffer {
    VAddr address;
    u32 image_size;
    u16 transfer_unit;
    u16 stride;
};

struct ConversionData {
    ConversionParameters params;
    /// Input parameters for the Y (luma) plane
    ConversionBuffer src_Y;
    /// Output parameters for the conversion results
    ConversionBuffer dst;
};

static ConversionData conversion;

static void SetInputFormat(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.params.input_format = static_cast<InputFormat>(cmd_buff[1]);
    LOG_DEBUG(Service_Y2R, "called input_format=%u", conversion.params.input_format);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetOutputFormat(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.params.output_format = static_cast<OutputFormat>(cmd_buff[1]);
    LOG_DEBUG(Service_Y2R, "called output_format=%u", conversion.params.output_format);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetRotation(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.params.rotation = static_cast<Rotation>(cmd_buff[1]);
    LOG_DEBUG(Service_Y2R, "called rotation=%u", conversion.params.rotation);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetBlockAlignment(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.params.block_alignment = static_cast<BlockAlignment>(cmd_buff[1]);
    LOG_DEBUG(Service_Y2R, "called alignment=%u", conversion.params.block_alignment);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

/**
 * Y2R_U::GetTransferEndEvent service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      3 : The handle of the completion event
 */
static void GetTransferEndEvent(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[3] = Kernel::g_handle_table.Create(completion_event).MoveFrom();
    LOG_DEBUG(Service_Y2R, "called");
}

static void SetSendingY(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.src_Y.address = cmd_buff[1];
    conversion.src_Y.image_size = cmd_buff[2];
    conversion.src_Y.transfer_unit = cmd_buff[3];
    conversion.src_Y.stride = cmd_buff[4];
    u32 src_process_handle = cmd_buff[6];
    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
        "src_process_handle=0x%08X", conversion.src_Y.image_size,
        conversion.src_Y.transfer_unit, conversion.src_Y.stride, src_process_handle);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetReceiving(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.dst.address = cmd_buff[1];
    conversion.dst.image_size = cmd_buff[2];
    conversion.dst.transfer_unit = cmd_buff[3];
    conversion.dst.stride = cmd_buff[4];
    u32 dst_process_handle = cmd_buff[6];
    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
        "dst_process_handle=0x%08X", conversion.dst.image_size,
        conversion.dst.transfer_unit, conversion.dst.stride,
        dst_process_handle);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetInputLineWidth(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.params.input_line_width = cmd_buff[1];
    LOG_DEBUG(Service_Y2R, "input_line_width=%u", conversion.params.input_line_width);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetInputLines(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.params.input_lines = cmd_buff[1];
    LOG_DEBUG(Service_Y2R, "input_line_number=%u", conversion.params.input_lines);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void StartConversion(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const ConversionParameters& params = conversion.params;

    const u8* srcY_buffer = Memory::GetPointer(conversion.src_Y.address);
    u8* dst_buffer = Memory::GetPointer(conversion.dst.address);

    // TODO: support color and other kinds of conversions
    ASSERT(params.input_format == InputFormat::YUV422_Indiv8
        || params.input_format == InputFormat::YUV420_Indiv8);
    ASSERT(params.output_format == OutputFormat::Rgb24);
    ASSERT(params.rotation == Rotation::None);
    const int bpp = 3;

    switch (params.block_alignment) {
    case BlockAlignment::Linear:
    {
        const size_t input_lines = params.input_lines;
        const size_t input_line_width = params.input_line_width;
        const size_t srcY_stride = conversion.src_Y.stride;
        const size_t dst_stride = conversion.dst.stride;

        size_t srcY_offset = 0;
        size_t dst_offset = 0;

        for (size_t line = 0; line < input_lines; ++line) {
            for (size_t i = 0; i < input_line_width; ++i) {
                u8 Y = srcY_buffer[srcY_offset];
                dst_buffer[dst_offset + 0] = Y;
                dst_buffer[dst_offset + 1] = Y;
                dst_buffer[dst_offset + 2] = Y;

                srcY_offset += 1;
                dst_offset += bpp;
            }
            srcY_offset += srcY_stride;
            dst_offset += dst_stride;
        }
        break;
    }
    case BlockAlignment::Block8x8:
    {
        const size_t input_lines = params.input_lines;
        const size_t input_line_width = params.input_line_width;
        const size_t srcY_stride = conversion.src_Y.stride;
        const size_t dst_transfer_unit = conversion.dst.transfer_unit;
        const size_t dst_stride = conversion.dst.stride;

        size_t srcY_offset = 0;
        size_t dst_tile_line_offs = 0;

        const size_t tile_size = 8 * 8 * bpp;

        for (size_t line = 0; line < input_lines;) {
            size_t max_line = line + 8;

            for (; line < max_line; ++line) {
                for (size_t x = 0; x < input_line_width; ++x) {
                    size_t tile_x = x / 8;

                    size_t dst_tile_offs = dst_tile_line_offs + tile_x * tile_size;
                    size_t tile_i = VideoCore::MortonInterleave((u32)x, (u32)line);

                    size_t dst_offset = dst_tile_offs + tile_i * bpp;

                    u8 Y = srcY_buffer[srcY_offset];
                    dst_buffer[dst_offset + 0] = Y;
                    dst_buffer[dst_offset + 1] = Y;
                    dst_buffer[dst_offset + 2] = Y;

                    srcY_offset += 1;
                }

                srcY_offset += srcY_stride;
            }

            dst_tile_line_offs += dst_transfer_unit + dst_stride;
        }
        break;
    }
    }

    // dst_image_size would seem to be perfect for this, but it doesn't include the stride :(
    u32 total_output_size = params.input_lines *
        (conversion.dst.transfer_unit + conversion.dst.stride);
    VideoCore::g_renderer->hw_rasterizer->NotifyFlush(
        Memory::VirtualToPhysicalAddress(conversion.dst.address), total_output_size);

    LOG_DEBUG(Service_Y2R, "called");
    completion_event->Signal();

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

/**
 * Y2R_U::IsBusyConversion service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : 1 if there's a conversion running, otherwise 0.
 */
static void IsBusyConversion(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // StartConversion always finishes immediately
    LOG_DEBUG(Service_Y2R, "called");
}

/**
 * Y2R_U::SetConversionParams service function
 */
static void SetConversionParams(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    auto params = reinterpret_cast<const ConversionParameters*>(&cmd_buff[1]);
    conversion.params = *params;

    cmd_buff[0] = 0x00290000; // TODO verify
    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_DEBUG(Service_Y2R,
        "called input_format=%hhu output_format=%hhu rotation=%hhu block_alignment=%hhu "
        "input_line_width=%hX input_lines=%hu standard_coefficient=%hhu reserved=%hhu alpha=%hX",
        params->input_format, params->output_format, params->rotation, params->block_alignment,
        params->input_line_width, params->input_lines, params->standard_coefficient);
}

static void PingProcess(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;
    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, SetInputFormat,          "SetInputFormat"},
    {0x00030040, SetOutputFormat,         "SetOutputFormat"},
    {0x00050040, SetRotation,             "SetRotation"},
    {0x00070040, SetBlockAlignment,       "SetBlockAlignment"},
    {0x000D0040, nullptr,                 "SetTransferEndInterrupt"},
    {0x000F0000, GetTransferEndEvent,     "GetTransferEndEvent"},
    {0x00100102, SetSendingY,             "SetSendingY"},
    {0x00110102, nullptr,                 "SetSendingU"},
    {0x00120102, nullptr,                 "SetSendingV"},
    {0x00180102, SetReceiving,            "SetReceiving"},
    {0x001A0040, SetInputLineWidth,       "SetInputLineWidth"},
    {0x001C0040, SetInputLines,           "SetInputLines"},
    {0x00200040, nullptr,                 "SetStandardCoefficient"},
    {0x00220040, nullptr,                 "SetAlpha"},
    {0x00260000, StartConversion,         "StartConversion"},
    {0x00270000, nullptr,                 "StopConversion"},
    {0x00280000, IsBusyConversion,        "IsBusyConversion"},
    {0x002901C0, SetConversionParams,     "SetConversionParams"},
    {0x002A0000, PingProcess,             "PingProcess"},
    {0x002B0000, nullptr,                 "DriverInitialize"},
    {0x002C0000, nullptr,                 "DriverFinalize"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    completion_event = Kernel::Event::Create(RESETTYPE_ONESHOT, "Y2R:Completed");
    std::memset(&conversion, 0, sizeof(conversion));

    Register(FunctionTable);
}

} // namespace
