// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/kernel.h"
#include "core/hle/service/y2r_u.h"
#include "core/hw/y2r.h"

namespace Service {
namespace Y2R {

struct ConversionParameters {
    InputFormat input_format;
    OutputFormat output_format;
    Rotation rotation;
    BlockAlignment block_alignment;
    u16 input_line_width;
    u16 input_lines;
    StandardCoefficient standard_coefficient;
    u8 padding;
    u16 alpha;
};
static_assert(sizeof(ConversionParameters) == 12, "ConversionParameters struct has incorrect size");

static Kernel::SharedPtr<Kernel::Event> completion_event;
static ConversionConfiguration conversion;
static DitheringWeightParams dithering_weight_params;
static u32 temporal_dithering_enabled = 0;
static u32 transfer_end_interrupt_enabled = 0;
static u32 spacial_dithering_enabled = 0;

static const CoefficientSet standard_coefficients[4] = {
    {{0x100, 0x166, 0xB6, 0x58, 0x1C5, -0x166F, 0x10EE, -0x1C5B}}, // ITU_Rec601
    {{0x100, 0x193, 0x77, 0x2F, 0x1DB, -0x1933, 0xA7C, -0x1D51}},  // ITU_Rec709
    {{0x12A, 0x198, 0xD0, 0x64, 0x204, -0x1BDE, 0x10F2, -0x229B}}, // ITU_Rec601_Scaling
    {{0x12A, 0x1CA, 0x88, 0x36, 0x21C, -0x1F04, 0x99C, -0x2421}},  // ITU_Rec709_Scaling
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

ResultCode ConversionConfiguration::SetStandardCoefficient(
    StandardCoefficient standard_coefficient) {
    size_t index = static_cast<size_t>(standard_coefficient);
    if (index >= ARRAY_SIZE(standard_coefficients)) {
        return ResultCode(ErrorDescription::InvalidEnumValue, ErrorModule::CAM,
                          ErrorSummary::InvalidArgument, ErrorLevel::Usage); // 0xE0E053ED
    }

    std::memcpy(coefficients.data(), standard_coefficients[index].data(), sizeof(coefficients));
    return RESULT_SUCCESS;
}

static void SetInputFormat(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.input_format = static_cast<InputFormat>(cmd_buff[1]);

    cmd_buff[0] = IPC::MakeHeader(0x1, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_Y2R, "called input_format=%hhu", conversion.input_format);
}

static void GetInputFormat(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x2, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = static_cast<u32>(conversion.input_format);

    LOG_DEBUG(Service_Y2R, "called input_format=%hhu", conversion.input_format);
}

static void SetOutputFormat(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.output_format = static_cast<OutputFormat>(cmd_buff[1]);

    cmd_buff[0] = IPC::MakeHeader(0x3, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_Y2R, "called output_format=%hhu", conversion.output_format);
}

static void GetOutputFormat(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x4, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = static_cast<u32>(conversion.output_format);

    LOG_DEBUG(Service_Y2R, "called output_format=%hhu", conversion.output_format);
}

static void SetRotation(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.rotation = static_cast<Rotation>(cmd_buff[1]);

    cmd_buff[0] = IPC::MakeHeader(0x5, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_Y2R, "called rotation=%hhu", conversion.rotation);
}

static void GetRotation(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x6, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = static_cast<u32>(conversion.rotation);

    LOG_DEBUG(Service_Y2R, "called rotation=%hhu", conversion.rotation);
}

static void SetBlockAlignment(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.block_alignment = static_cast<BlockAlignment>(cmd_buff[1]);

    cmd_buff[0] = IPC::MakeHeader(0x7, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_Y2R, "called block_alignment=%hhu", conversion.block_alignment);
}

static void GetBlockAlignment(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x8, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = static_cast<u32>(conversion.block_alignment);

    LOG_DEBUG(Service_Y2R, "called block_alignment=%hhu", conversion.block_alignment);
}

/**
 * Y2R_U::SetSpacialDithering service function
 *  Inputs:
 *      1 : u8, 0 = Disabled, 1 = Enabled
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SetSpacialDithering(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    spacial_dithering_enabled = cmd_buff[1] & 0xF;

    cmd_buff[0] = IPC::MakeHeader(0x9, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R_U::GetSpacialDithering service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8, 0 = Disabled, 1 = Enabled
 */
static void GetSpacialDithering(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0xA, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = spacial_dithering_enabled;

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R_U::SetTemporalDithering service function
 *  Inputs:
 *      1 : u8, 0 = Disabled, 1 = Enabled
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SetTemporalDithering(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    temporal_dithering_enabled = cmd_buff[1] & 0xF;

    cmd_buff[0] = IPC::MakeHeader(0xB, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R_U::GetTemporalDithering service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8, 0 = Disabled, 1 = Enabled
 */
static void GetTemporalDithering(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0xC, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = temporal_dithering_enabled;

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R_U::SetTransferEndInterrupt service function
 *  Inputs:
 *      1 : u8, 0 = Disabled, 1 = Enabled
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SetTransferEndInterrupt(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();
    transfer_end_interrupt_enabled = cmd_buff[1] & 0xf;

    cmd_buff[0] = IPC::MakeHeader(0xD, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R_U::GetTransferEndInterrupt service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8, 0 = Disabled, 1 = Enabled
 */
static void GetTransferEndInterrupt(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0xE, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = transfer_end_interrupt_enabled;

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R_U::GetTransferEndEvent service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      3 : The handle of the completion event
 */
static void GetTransferEndEvent(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0xF, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[3] = Kernel::g_handle_table.Create(completion_event).MoveFrom();

    LOG_DEBUG(Service_Y2R, "called");
}

static void SetSendingY(Interface* self) {
    // The helper should be passed by argument to the function
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x00100102);
    conversion.src_Y.address = rp.Pop<u32>();
    conversion.src_Y.image_size = rp.Pop<u32>();
    conversion.src_Y.transfer_unit = rp.Pop<u32>();
    conversion.src_Y.gap = rp.Pop<u32>();
    Kernel::Handle src_process_handle = rp.PopHandle();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
                           "src_process_handle=0x%08X",
              conversion.src_Y.image_size, conversion.src_Y.transfer_unit, conversion.src_Y.gap,
              src_process_handle);
}

static void SetSendingU(Interface* self) {
    // The helper should be passed by argument to the function
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x00110102);
    conversion.src_U.address = rp.Pop<u32>();
    conversion.src_U.image_size = rp.Pop<u32>();
    conversion.src_U.transfer_unit = rp.Pop<u32>();
    conversion.src_U.gap = rp.Pop<u32>();
    Kernel::Handle src_process_handle = rp.PopHandle();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
                           "src_process_handle=0x%08X",
              conversion.src_U.image_size, conversion.src_U.transfer_unit, conversion.src_U.gap,
              src_process_handle);
}

static void SetSendingV(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.src_V.address = cmd_buff[1];
    conversion.src_V.image_size = cmd_buff[2];
    conversion.src_V.transfer_unit = cmd_buff[3];
    conversion.src_V.gap = cmd_buff[4];

    cmd_buff[0] = IPC::MakeHeader(0x12, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
                           "src_process_handle=0x%08X",
              conversion.src_V.image_size, conversion.src_V.transfer_unit, conversion.src_V.gap,
              cmd_buff[6]);
}

static void SetSendingYUYV(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.src_YUYV.address = cmd_buff[1];
    conversion.src_YUYV.image_size = cmd_buff[2];
    conversion.src_YUYV.transfer_unit = cmd_buff[3];
    conversion.src_YUYV.gap = cmd_buff[4];

    cmd_buff[0] = IPC::MakeHeader(0x13, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
                           "src_process_handle=0x%08X",
              conversion.src_YUYV.image_size, conversion.src_YUYV.transfer_unit,
              conversion.src_YUYV.gap, cmd_buff[6]);
}

/**
 * Y2R::IsFinishedSendingYuv service function
 * Output:
 *       1 : Result of the function, 0 on success, otherwise error code
 *       2 : u8, 0 = Not Finished, 1 = Finished
 */
static void IsFinishedSendingYuv(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x14, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 1;

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R::IsFinishedSendingY service function
 * Output:
 *       1 : Result of the function, 0 on success, otherwise error code
 *       2 : u8, 0 = Not Finished, 1 = Finished
 */
static void IsFinishedSendingY(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x15, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 1;

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R::IsFinishedSendingU service function
 * Output:
 *       1 : Result of the function, 0 on success, otherwise error code
 *       2 : u8, 0 = Not Finished, 1 = Finished
 */
static void IsFinishedSendingU(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x16, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 1;

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R::IsFinishedSendingV service function
 * Output:
 *       1 : Result of the function, 0 on success, otherwise error code
 *       2 : u8, 0 = Not Finished, 1 = Finished
 */
static void IsFinishedSendingV(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x17, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 1;

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

static void SetReceiving(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.dst.address = cmd_buff[1];
    conversion.dst.image_size = cmd_buff[2];
    conversion.dst.transfer_unit = cmd_buff[3];
    conversion.dst.gap = cmd_buff[4];

    cmd_buff[0] = IPC::MakeHeader(0x18, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
                           "dst_process_handle=0x%08X",
              conversion.dst.image_size, conversion.dst.transfer_unit, conversion.dst.gap,
              cmd_buff[6]);
}

/**
 * Y2R::IsFinishedReceiving service function
 * Output:
 *       1 : Result of the function, 0 on success, otherwise error code
 *       2 : u8, 0 = Not Finished, 1 = Finished
 */
static void IsFinishedReceiving(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x19, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 1;

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

static void SetInputLineWidth(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x1A, 1, 0);
    cmd_buff[1] = conversion.SetInputLineWidth(cmd_buff[1]).raw;

    LOG_DEBUG(Service_Y2R, "called input_line_width=%u", cmd_buff[1]);
}

static void GetInputLineWidth(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x1B, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = conversion.input_line_width;

    LOG_DEBUG(Service_Y2R, "called input_line_width=%u", conversion.input_line_width);
}

static void SetInputLines(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x1C, 1, 0);
    cmd_buff[1] = conversion.SetInputLines(cmd_buff[1]).raw;

    LOG_DEBUG(Service_Y2R, "called input_lines=%u", cmd_buff[1]);
}

static void GetInputLines(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x1D, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = static_cast<u32>(conversion.input_lines);

    LOG_DEBUG(Service_Y2R, "called input_lines=%u", conversion.input_lines);
}

static void SetCoefficient(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const u16* coefficients = reinterpret_cast<const u16*>(&cmd_buff[1]);
    std::memcpy(conversion.coefficients.data(), coefficients, sizeof(CoefficientSet));

    cmd_buff[0] = IPC::MakeHeader(0x1E, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_Y2R, "called coefficients=[%hX, %hX, %hX, %hX, %hX, %hX, %hX, %hX]",
              coefficients[0], coefficients[1], coefficients[2], coefficients[3], coefficients[4],
              coefficients[5], coefficients[6], coefficients[7]);
}

static void GetCoefficient(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x1F, 5, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    std::memcpy(&cmd_buff[2], conversion.coefficients.data(), sizeof(CoefficientSet));

    LOG_DEBUG(Service_Y2R, "called");
}

static void SetStandardCoefficient(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 index = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0x20, 1, 0);
    cmd_buff[1] = conversion.SetStandardCoefficient((StandardCoefficient)index).raw;

    LOG_DEBUG(Service_Y2R, "called standard_coefficient=%u", index);
}

static void GetStandardCoefficient(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u32 index = cmd_buff[1];

    if (index < ARRAY_SIZE(standard_coefficients)) {
        cmd_buff[0] = IPC::MakeHeader(0x21, 5, 0);
        cmd_buff[1] = RESULT_SUCCESS.raw;
        std::memcpy(&cmd_buff[2], &standard_coefficients[index], sizeof(CoefficientSet));

        LOG_DEBUG(Service_Y2R, "called standard_coefficient=%u ", index);
    } else {
        cmd_buff[0] = IPC::MakeHeader(0x21, 1, 0);
        cmd_buff[1] = ResultCode(ErrorDescription::InvalidEnumValue, ErrorModule::CAM,
                                 ErrorSummary::InvalidArgument, ErrorLevel::Usage)
                          .raw;

        LOG_ERROR(Service_Y2R, "called standard_coefficient=%u  The argument is invalid!", index);
    }
}

static void SetAlpha(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    conversion.alpha = cmd_buff[1];

    cmd_buff[0] = IPC::MakeHeader(0x22, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_Y2R, "called alpha=%hu", conversion.alpha);
}

static void GetAlpha(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x23, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = conversion.alpha;

    LOG_DEBUG(Service_Y2R, "called alpha=%hu", conversion.alpha);
}

static void SetDitheringWeightParams(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x24, 8, 0); // 0x240200
    rp.PopRaw(dithering_weight_params);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called");
}

static void GetDitheringWeightParams(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x25, 9, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    std::memcpy(&cmd_buff[2], &dithering_weight_params, sizeof(DitheringWeightParams));

    LOG_DEBUG(Service_Y2R, "called");
}

static void StartConversion(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // dst_image_size would seem to be perfect for this, but it doesn't include the gap :(
    u32 total_output_size =
        conversion.input_lines * (conversion.dst.transfer_unit + conversion.dst.gap);
    Memory::RasterizerFlushAndInvalidateRegion(
        Memory::VirtualToPhysicalAddress(conversion.dst.address), total_output_size);

    HW::Y2R::PerformConversion(conversion);

    completion_event->Signal();

    cmd_buff[0] = IPC::MakeHeader(0x26, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_Y2R, "called");
}

static void StopConversion(Interface* self) {
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
static void IsBusyConversion(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x28, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0; // StartConversion always finishes immediately

    LOG_DEBUG(Service_Y2R, "called");
}

/**
 * Y2R_U::SetPackageParameter service function
 */
static void SetPackageParameter(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    auto params = reinterpret_cast<const ConversionParameters*>(&cmd_buff[1]);

    conversion.input_format = params->input_format;
    conversion.output_format = params->output_format;
    conversion.rotation = params->rotation;
    conversion.block_alignment = params->block_alignment;

    ResultCode result = conversion.SetInputLineWidth(params->input_line_width);

    if (result.IsError())
        goto cleanup;

    result = conversion.SetInputLines(params->input_lines);

    if (result.IsError())
        goto cleanup;

    result = conversion.SetStandardCoefficient(params->standard_coefficient);

    if (result.IsError())
        goto cleanup;

    conversion.padding = params->padding;
    conversion.alpha = params->alpha;

cleanup:
    cmd_buff[0] = IPC::MakeHeader(0x29, 1, 0);
    cmd_buff[1] = result.raw;

    LOG_DEBUG(
        Service_Y2R,
        "called input_format=%hhu output_format=%hhu rotation=%hhu block_alignment=%hhu "
        "input_line_width=%hu input_lines=%hu standard_coefficient=%hhu reserved=%hhu alpha=%hX",
        params->input_format, params->output_format, params->rotation, params->block_alignment,
        params->input_line_width, params->input_lines, params->standard_coefficient,
        params->padding, params->alpha);
}

static void PingProcess(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x2A, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

static void DriverInitialize(Interface* self) {
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

static void DriverFinalize(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x2C, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_Y2R, "called");
}

static void GetPackageParameter(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x2D, 4, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    std::memcpy(&cmd_buff[2], &conversion, sizeof(ConversionParameters));

    LOG_DEBUG(Service_Y2R, "called");
}

const Interface::FunctionInfo FunctionTable[] = {
    {0x00010040, SetInputFormat, "SetInputFormat"},
    {0x00020000, GetInputFormat, "GetInputFormat"},
    {0x00030040, SetOutputFormat, "SetOutputFormat"},
    {0x00040000, GetOutputFormat, "GetOutputFormat"},
    {0x00050040, SetRotation, "SetRotation"},
    {0x00060000, GetRotation, "GetRotation"},
    {0x00070040, SetBlockAlignment, "SetBlockAlignment"},
    {0x00080000, GetBlockAlignment, "GetBlockAlignment"},
    {0x00090040, SetSpacialDithering, "SetSpacialDithering"},
    {0x000A0000, GetSpacialDithering, "GetSpacialDithering"},
    {0x000B0040, SetTemporalDithering, "SetTemporalDithering"},
    {0x000C0000, GetTemporalDithering, "GetTemporalDithering"},
    {0x000D0040, SetTransferEndInterrupt, "SetTransferEndInterrupt"},
    {0x000E0000, GetTransferEndInterrupt, "GetTransferEndInterrupt"},
    {0x000F0000, GetTransferEndEvent, "GetTransferEndEvent"},
    {0x00100102, SetSendingY, "SetSendingY"},
    {0x00110102, SetSendingU, "SetSendingU"},
    {0x00120102, SetSendingV, "SetSendingV"},
    {0x00130102, SetSendingYUYV, "SetSendingYUYV"},
    {0x00140000, IsFinishedSendingYuv, "IsFinishedSendingYuv"},
    {0x00150000, IsFinishedSendingY, "IsFinishedSendingY"},
    {0x00160000, IsFinishedSendingU, "IsFinishedSendingU"},
    {0x00170000, IsFinishedSendingV, "IsFinishedSendingV"},
    {0x00180102, SetReceiving, "SetReceiving"},
    {0x00190000, IsFinishedReceiving, "IsFinishedReceiving"},
    {0x001A0040, SetInputLineWidth, "SetInputLineWidth"},
    {0x001B0000, GetInputLineWidth, "GetInputLineWidth"},
    {0x001C0040, SetInputLines, "SetInputLines"},
    {0x001D0000, GetInputLines, "GetInputLines"},
    {0x001E0100, SetCoefficient, "SetCoefficient"},
    {0x001F0000, GetCoefficient, "GetCoefficient"},
    {0x00200040, SetStandardCoefficient, "SetStandardCoefficient"},
    {0x00210040, GetStandardCoefficient, "GetStandardCoefficient"},
    {0x00220040, SetAlpha, "SetAlpha"},
    {0x00230000, GetAlpha, "GetAlpha"},
    {0x00240200, SetDitheringWeightParams, "SetDitheringWeightParams"},
    {0x00250000, GetDitheringWeightParams, "GetDitheringWeightParams"},
    {0x00260000, StartConversion, "StartConversion"},
    {0x00270000, StopConversion, "StopConversion"},
    {0x00280000, IsBusyConversion, "IsBusyConversion"},
    {0x002901C0, SetPackageParameter, "SetPackageParameter"},
    {0x002A0000, PingProcess, "PingProcess"},
    {0x002B0000, DriverInitialize, "DriverInitialize"},
    {0x002C0000, DriverFinalize, "DriverFinalize"},
    {0x002D0000, GetPackageParameter, "GetPackageParameter"},
};

Y2R_U::Y2R_U() {
    completion_event = Kernel::Event::Create(Kernel::ResetType::OneShot, "Y2R:Completed");
    std::memset(&conversion, 0, sizeof(conversion));

    Register(FunctionTable);
}

Y2R_U::~Y2R_U() {
    completion_event = nullptr;
}

} // namespace Y2R
} // namespace Service