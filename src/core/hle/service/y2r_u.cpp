// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>

#include "common/logging/log.h"

#include "core/hle/hle.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/y2r_u.h"
#include "core/hw/y2r.h"
#include "core/mem_map.h"

#include "video_core/renderer_base.h"
#include "video_core/utils.h"
#include "video_core/video_core.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// Namespace Y2R_U

namespace Y2R_U {

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

static Kernel::SharedPtr<Kernel::Event> completion_event;
static ConversionConfiguration conversion;

static const CoefficientSet standard_coefficients[4] = {
    {{ 0x100, 0x166, 0xB6, 0x58, 0x1C5, -0x166F, 0x10EE, -0x1C5B }}, // ITU_Rec601
    {{ 0x100, 0x193, 0x77, 0x2F, 0x1DB, -0x1933,  0xA7C, -0x1D51 }}, // ITU_Rec709
    {{ 0x12A, 0x198, 0xD0, 0x64, 0x204, -0x1BDE, 0x10F2, -0x229B }}, // ITU_Rec601_Scaling
    {{ 0x12A, 0x1CA, 0x88, 0x36, 0x21C, -0x1F04,  0x99C, -0x2421 }}, // ITU_Rec709_Scaling
};

ResultCode ConversionConfiguration::SetInputLineWidth(u16 width) {
    if (width == 0 || width > 1024 || width % 8 != 0) {
        return ResultCode(ErrorDescription::OutOfRange, ErrorModule::CAM,
            ErrorSummary::InvalidArgument, ErrorLevel::Usage); // 0xE0E053FD
    }

    // Note: The hardware uses the register value 0 to represent a width of 1024, so for a width of
    // 1024 the `camera` module would set the value 0 here, but we don't need to emulate this
    // internal detail.
    this->input_line_width = width;
    return RESULT_SUCCESS;
}

ResultCode ConversionConfiguration::SetInputLines(u16 lines) {
    if (lines == 0 || lines > 1024) {
        return ResultCode(ErrorDescription::OutOfRange, ErrorModule::CAM,
            ErrorSummary::InvalidArgument, ErrorLevel::Usage); // 0xE0E053FD
    }

    // Note: In what appears to be a bug, the `camera` module does not set the hardware register at
    // all if `lines` is 1024, so the conversion uses the last value that was set. The intention
    // was probably to set it to 0 like in SetInputLineWidth.
    if (lines != 1024) {
        this->input_lines = lines;
    }
    return RESULT_SUCCESS;
}

ResultCode ConversionConfiguration::SetStandardCoefficient(StandardCoefficient standard_coefficient) {
    size_t index = static_cast<size_t>(standard_coefficient);
    if (index >= 4) {
        return ResultCode(ErrorDescription::InvalidEnumValue, ErrorModule::CAM,
            ErrorSummary::InvalidArgument, ErrorLevel::Usage); // 0xE0E053ED
    }

    std::memcpy(coefficients.data(), standard_coefficients[index].data(), sizeof(coefficients));
    return RESULT_SUCCESS;
}

static void SetInputFormat(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.input_format = static_cast<InputFormat>(cmd_buff[1]);
    LOG_DEBUG(Service_Y2R, "called input_format=%hhu", conversion.input_format);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetOutputFormat(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.output_format = static_cast<OutputFormat>(cmd_buff[1]);
    LOG_DEBUG(Service_Y2R, "called output_format=%hhu", conversion.output_format);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetRotation(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.rotation = static_cast<Rotation>(cmd_buff[1]);
    LOG_DEBUG(Service_Y2R, "called rotation=%hhu", conversion.rotation);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetBlockAlignment(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.block_alignment = static_cast<BlockAlignment>(cmd_buff[1]);
    LOG_DEBUG(Service_Y2R, "called alignment=%hhu", conversion.block_alignment);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetTransferEndInterrupt(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0xD, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_DEBUG(Service_Y2R, "(STUBBED) called");
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
    conversion.src_Y.gap = cmd_buff[4];
    u32 src_process_handle = cmd_buff[6];
    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
        "src_process_handle=0x%08X", conversion.src_Y.image_size,
        conversion.src_Y.transfer_unit, conversion.src_Y.gap, src_process_handle);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetSendingU(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.src_U.address = cmd_buff[1];
    conversion.src_U.image_size = cmd_buff[2];
    conversion.src_U.transfer_unit = cmd_buff[3];
    conversion.src_U.gap = cmd_buff[4];
    u32 src_process_handle = cmd_buff[6];
    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
        "src_process_handle=0x%08X", conversion.src_U.image_size,
        conversion.src_U.transfer_unit, conversion.src_U.gap, src_process_handle);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetSendingV(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.src_V.address = cmd_buff[1];
    conversion.src_V.image_size = cmd_buff[2];
    conversion.src_V.transfer_unit = cmd_buff[3];
    conversion.src_V.gap = cmd_buff[4];
    u32 src_process_handle = cmd_buff[6];
    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
        "src_process_handle=0x%08X", conversion.src_V.image_size,
        conversion.src_V.transfer_unit, conversion.src_V.gap, src_process_handle);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetSendingYUYV(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.src_YUYV.address = cmd_buff[1];
    conversion.src_YUYV.image_size = cmd_buff[2];
    conversion.src_YUYV.transfer_unit = cmd_buff[3];
    conversion.src_YUYV.gap = cmd_buff[4];
    u32 src_process_handle = cmd_buff[6];
    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
        "src_process_handle=0x%08X", conversion.src_YUYV.image_size,
        conversion.src_YUYV.transfer_unit, conversion.src_YUYV.gap, src_process_handle);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetReceiving(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.dst.address = cmd_buff[1];
    conversion.dst.image_size = cmd_buff[2];
    conversion.dst.transfer_unit = cmd_buff[3];
    conversion.dst.gap = cmd_buff[4];
    u32 dst_process_handle = cmd_buff[6];
    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
        "dst_process_handle=0x%08X", conversion.dst.image_size,
        conversion.dst.transfer_unit, conversion.dst.gap,
        dst_process_handle);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetInputLineWidth(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    LOG_DEBUG(Service_Y2R, "called input_line_width=%u", cmd_buff[1]);
    cmd_buff[1] = conversion.SetInputLineWidth(cmd_buff[1]).raw;
}

static void SetInputLines(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    LOG_DEBUG(Service_Y2R, "called input_line_number=%u", cmd_buff[1]);
    cmd_buff[1] = conversion.SetInputLines(cmd_buff[1]).raw;
}

static void SetCoefficient(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const u16* coefficients = reinterpret_cast<const u16*>(&cmd_buff[1]);
    std::memcpy(conversion.coefficients.data(), coefficients, sizeof(CoefficientSet));
    LOG_DEBUG(Service_Y2R, "called coefficients=[%hX, %hX, %hX, %hX, %hX, %hX, %hX, %hX]",
            coefficients[0], coefficients[1], coefficients[2], coefficients[3],
            coefficients[4], coefficients[5], coefficients[6], coefficients[7]);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void SetStandardCoefficient(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    LOG_DEBUG(Service_Y2R, "called standard_coefficient=%u", cmd_buff[1]);

    cmd_buff[1] = conversion.SetStandardCoefficient((StandardCoefficient)cmd_buff[1]).raw;
}

static void SetAlpha(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.alpha = cmd_buff[1];
    LOG_DEBUG(Service_Y2R, "called alpha=%hu", conversion.alpha);

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void StartConversion(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    HW::Y2R::PerformConversion(conversion);

    // dst_image_size would seem to be perfect for this, but it doesn't include the gap :(
    u32 total_output_size = conversion.input_lines *
        (conversion.dst.transfer_unit + conversion.dst.gap);
    VideoCore::g_renderer->hw_rasterizer->NotifyFlush(
        Memory::VirtualToPhysicalAddress(conversion.dst.address), total_output_size);

    LOG_DEBUG(Service_Y2R, "called");
    completion_event->Signal();

    cmd_buff[1] = RESULT_SUCCESS.raw;
}

static void StopConversion(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x27, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_DEBUG(Service_Y2R, "called");
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
    LOG_DEBUG(Service_Y2R,
        "called input_format=%hhu output_format=%hhu rotation=%hhu block_alignment=%hhu "
        "input_line_width=%hu input_lines=%hu standard_coefficient=%hhu "
        "reserved=%hhu alpha=%hX",
        params->input_format, params->output_format, params->rotation, params->block_alignment,
        params->input_line_width, params->input_lines, params->standard_coefficient,
        params->reserved, params->alpha);

    ResultCode result = RESULT_SUCCESS;

    conversion.input_format = params->input_format;
    conversion.output_format = params->output_format;
    conversion.rotation = params->rotation;
    conversion.block_alignment = params->block_alignment;
    result = conversion.SetInputLineWidth(params->input_line_width);
    if (result.IsError()) goto cleanup;
    result = conversion.SetInputLines(params->input_lines);
    if (result.IsError()) goto cleanup;
    result = conversion.SetStandardCoefficient(params->standard_coefficient);
    if (result.IsError()) goto cleanup;
    conversion.alpha = params->alpha;

cleanup:
    cmd_buff[0] = IPC::MakeHeader(0x29, 1, 0);
    cmd_buff[1] = result.raw;
}

static void PingProcess(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;
    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

static void DriverInitialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.input_format = InputFormat::YUV422_Indiv8;
    conversion.output_format = OutputFormat::RGBA8;
    conversion.rotation = Rotation::None;
    conversion.block_alignment = BlockAlignment::Linear;
    conversion.coefficients.fill(0);
    conversion.SetInputLineWidth(1024);
    conversion.SetInputLines(1024);
    conversion.alpha = 0;

    ConversionBuffer zero_buffer = {};
    conversion.src_Y = zero_buffer;
    conversion.src_U = zero_buffer;
    conversion.src_V = zero_buffer;
    conversion.dst = zero_buffer;

    completion_event->Clear();

    cmd_buff[0] = IPC::MakeHeader(0x2B, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_DEBUG(Service_Y2R, "called");
}

static void DriverFinalize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x2C, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    LOG_DEBUG(Service_Y2R, "called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, SetInputFormat,          "SetInputFormat"},
    {0x00030040, SetOutputFormat,         "SetOutputFormat"},
    {0x00050040, SetRotation,             "SetRotation"},
    {0x00070040, SetBlockAlignment,       "SetBlockAlignment"},
    {0x000D0040, SetTransferEndInterrupt, "SetTransferEndInterrupt"},
    {0x000F0000, GetTransferEndEvent,     "GetTransferEndEvent"},
    {0x00100102, SetSendingY,             "SetSendingY"},
    {0x00110102, SetSendingU,             "SetSendingU"},
    {0x00120102, SetSendingV,             "SetSendingV"},
    {0x00130102, SetSendingYUYV,          "SetSendingYUYV"},
    {0x00180102, SetReceiving,            "SetReceiving"},
    {0x001A0040, SetInputLineWidth,       "SetInputLineWidth"},
    {0x001C0040, SetInputLines,           "SetInputLines"},
    {0x001E0100, SetCoefficient,          "SetCoefficient"},
    {0x00200040, SetStandardCoefficient,  "SetStandardCoefficient"},
    {0x00220040, SetAlpha,                "SetAlpha"},
    {0x00260000, StartConversion,         "StartConversion"},
    {0x00270000, StopConversion,          "StopConversion"},
    {0x00280000, IsBusyConversion,        "IsBusyConversion"},
    {0x002901C0, SetConversionParams,     "SetConversionParams"},
    {0x002A0000, PingProcess,             "PingProcess"},
    {0x002B0000, DriverInitialize,        "DriverInitialize"},
    {0x002C0000, DriverFinalize,          "DriverFinalize"},
};

////////////////////////////////////////////////////////////////////////////////////////////////////
// Interface class

Interface::Interface() {
    completion_event = Kernel::Event::Create(RESETTYPE_ONESHOT, "Y2R:Completed");
    std::memset(&conversion, 0, sizeof(conversion));

    Register(FunctionTable);
}

Interface::~Interface() {
    completion_event = nullptr;
}

} // namespace
