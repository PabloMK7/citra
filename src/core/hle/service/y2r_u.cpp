// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/logging/log.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
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
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1, 1, 0);

    conversion.input_format = rp.PopEnum<InputFormat>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called input_format=%hhu", static_cast<u8>(conversion.input_format));
}

static void GetInputFormat(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x2, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(conversion.input_format);

    LOG_DEBUG(Service_Y2R, "called input_format=%hhu", static_cast<u8>(conversion.input_format));
}

static void SetOutputFormat(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x3, 1, 0);

    conversion.output_format = rp.PopEnum<OutputFormat>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called output_format=%hhu", static_cast<u8>(conversion.output_format));
}

static void GetOutputFormat(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x4, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(conversion.output_format);

    LOG_DEBUG(Service_Y2R, "called output_format=%hhu", static_cast<u8>(conversion.output_format));
}

static void SetRotation(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x5, 1, 0);

    conversion.rotation = rp.PopEnum<Rotation>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called rotation=%hhu", static_cast<u8>(conversion.rotation));
}

static void GetRotation(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x6, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(conversion.rotation);

    LOG_DEBUG(Service_Y2R, "called rotation=%hhu", static_cast<u8>(conversion.rotation));
}

static void SetBlockAlignment(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x7, 1, 0);

    conversion.block_alignment = rp.PopEnum<BlockAlignment>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called block_alignment=%hhu",
              static_cast<u8>(conversion.block_alignment));
}

static void GetBlockAlignment(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x8, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.PushEnum(conversion.block_alignment);

    LOG_DEBUG(Service_Y2R, "called block_alignment=%hhu",
              static_cast<u8>(conversion.block_alignment));
}

/**
 * Y2R_U::SetSpacialDithering service function
 *  Inputs:
 *      1 : u8, 0 = Disabled, 1 = Enabled
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 */
static void SetSpacialDithering(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x9, 1, 0);

    spacial_dithering_enabled = rp.Pop<u8>() & 0xF;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R_U::GetSpacialDithering service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8, 0 = Disabled, 1 = Enabled
 */
static void GetSpacialDithering(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xA, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(spacial_dithering_enabled != 0);

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
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xB, 1, 0);
    temporal_dithering_enabled = rp.Pop<u8>() & 0xF;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R_U::GetTemporalDithering service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8, 0 = Disabled, 1 = Enabled
 */
static void GetTemporalDithering(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xC, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(temporal_dithering_enabled);

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
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xD, 1, 0);
    transfer_end_interrupt_enabled = rp.Pop<u8>() & 0xF;

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R_U::GetTransferEndInterrupt service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : u8, 0 = Disabled, 1 = Enabled
 */
static void GetTransferEndInterrupt(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xE, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(transfer_end_interrupt_enabled);

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R_U::GetTransferEndEvent service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      3 : The handle of the completion event
 */
static void GetTransferEndEvent(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0xF, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    rb.Push(RESULT_SUCCESS);
    rb.PushCopyHandles(Kernel::g_handle_table.Create(completion_event).Unwrap());

    LOG_DEBUG(Service_Y2R, "called");
}

static void SetSendingY(Interface* self) {
    // The helper should be passed by argument to the function
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x10, 4, 2);
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
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x11, 4, 2);
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
    // The helper should be passed by argument to the function
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x12, 4, 2);

    conversion.src_V.address = rp.Pop<u32>();
    conversion.src_V.image_size = rp.Pop<u32>();
    conversion.src_V.transfer_unit = rp.Pop<u32>();
    conversion.src_V.gap = rp.Pop<u32>();
    Kernel::Handle src_process_handle = rp.PopHandle();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
                           "src_process_handle=0x%08X",
              conversion.src_V.image_size, conversion.src_V.transfer_unit, conversion.src_V.gap,
              static_cast<u32>(src_process_handle));
}

static void SetSendingYUYV(Interface* self) {
    // The helper should be passed by argument to the function
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x13, 4, 2);

    conversion.src_YUYV.address = rp.Pop<u32>();
    conversion.src_YUYV.image_size = rp.Pop<u32>();
    conversion.src_YUYV.transfer_unit = rp.Pop<u32>();
    conversion.src_YUYV.gap = rp.Pop<u32>();
    Kernel::Handle src_process_handle = rp.PopHandle();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
                           "src_process_handle=0x%08X",
              conversion.src_YUYV.image_size, conversion.src_YUYV.transfer_unit,
              conversion.src_YUYV.gap, static_cast<u32>(src_process_handle));
}

/**
 * Y2R::IsFinishedSendingYuv service function
 * Output:
 *       1 : Result of the function, 0 on success, otherwise error code
 *       2 : u8, 0 = Not Finished, 1 = Finished
 */
static void IsFinishedSendingYuv(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x14, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(1);

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R::IsFinishedSendingY service function
 * Output:
 *       1 : Result of the function, 0 on success, otherwise error code
 *       2 : u8, 0 = Not Finished, 1 = Finished
 */
static void IsFinishedSendingY(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x15, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(1);

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R::IsFinishedSendingU service function
 * Output:
 *       1 : Result of the function, 0 on success, otherwise error code
 *       2 : u8, 0 = Not Finished, 1 = Finished
 */
static void IsFinishedSendingU(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x16, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(1);

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

/**
 * Y2R::IsFinishedSendingV service function
 * Output:
 *       1 : Result of the function, 0 on success, otherwise error code
 *       2 : u8, 0 = Not Finished, 1 = Finished
 */
static void IsFinishedSendingV(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x17, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(1);

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

static void SetReceiving(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x18, 4, 2);

    conversion.dst.address = rp.Pop<u32>();
    conversion.dst.image_size = rp.Pop<u32>();
    conversion.dst.transfer_unit = rp.Pop<u32>();
    conversion.dst.gap = rp.Pop<u32>();
    Kernel::Handle dst_process_handle = rp.PopHandle();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called image_size=0x%08X, transfer_unit=%hu, transfer_stride=%hu, "
                           "dst_process_handle=0x%08X",
              conversion.dst.image_size, conversion.dst.transfer_unit, conversion.dst.gap,
              static_cast<u32>(dst_process_handle));
}

/**
 * Y2R::IsFinishedReceiving service function
 * Output:
 *       1 : Result of the function, 0 on success, otherwise error code
 *       2 : u8, 0 = Not Finished, 1 = Finished
 */
static void IsFinishedReceiving(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x19, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(1);

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

static void SetInputLineWidth(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1A, 1, 0);
    u32 input_line_width = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(conversion.SetInputLineWidth(input_line_width));

    LOG_DEBUG(Service_Y2R, "called input_line_width=%u", input_line_width);
}

static void GetInputLineWidth(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1B, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(conversion.input_line_width);

    LOG_DEBUG(Service_Y2R, "called input_line_width=%u", conversion.input_line_width);
}

static void SetInputLines(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1C, 1, 0);
    u32 input_lines = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(conversion.SetInputLines(input_lines));

    LOG_DEBUG(Service_Y2R, "called input_lines=%u", input_lines);
}

static void GetInputLines(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1D, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(static_cast<u32>(conversion.input_lines));

    LOG_DEBUG(Service_Y2R, "called input_lines=%u", conversion.input_lines);
}

static void SetCoefficient(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x1E, 4, 0);

    rp.PopRaw<CoefficientSet>(conversion.coefficients);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called coefficients=[%hX, %hX, %hX, %hX, %hX, %hX, %hX, %hX]",
              conversion.coefficients[0], conversion.coefficients[1], conversion.coefficients[2],
              conversion.coefficients[3], conversion.coefficients[4], conversion.coefficients[5],
              conversion.coefficients[6], conversion.coefficients[7]);
}

static void GetCoefficient(Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x1F, 5, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    std::memcpy(&cmd_buff[2], conversion.coefficients.data(), sizeof(CoefficientSet));

    LOG_DEBUG(Service_Y2R, "called");
}

static void SetStandardCoefficient(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x20, 1, 0);
    u32 index = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(conversion.SetStandardCoefficient(static_cast<StandardCoefficient>(index)));

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
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x22, 1, 0);
    conversion.alpha = rp.Pop<u32>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called alpha=%hu", conversion.alpha);
}

static void GetAlpha(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x23, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push(conversion.alpha);

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
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x26, 0, 0);

    // dst_image_size would seem to be perfect for this, but it doesn't include the gap :(
    u32 total_output_size =
        conversion.input_lines * (conversion.dst.transfer_unit + conversion.dst.gap);
    Memory::RasterizerFlushVirtualRegion(conversion.dst.address, total_output_size,
                                         Memory::FlushMode::FlushAndInvalidate);

    HW::Y2R::PerformConversion(conversion);

    completion_event->Signal();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called");
}

static void StopConversion(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x27, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called");
}

/**
 * Y2R_U::IsBusyConversion service function
 *  Outputs:
 *      1 : Result of function, 0 on success, otherwise error code
 *      2 : 1 if there's a conversion running, otherwise 0.
 */
static void IsBusyConversion(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x28, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(0); // StartConversion always finishes immediately

    LOG_DEBUG(Service_Y2R, "called");
}

/**
 * Y2R_U::SetPackageParameter service function
 */
static void SetPackageParameter(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x29, 7, 0);
    auto params = rp.PopRaw<ConversionParameters>();

    conversion.input_format = params.input_format;
    conversion.output_format = params.output_format;
    conversion.rotation = params.rotation;
    conversion.block_alignment = params.block_alignment;

    ResultCode result = conversion.SetInputLineWidth(params.input_line_width);

    if (result.IsError())
        goto cleanup;

    result = conversion.SetInputLines(params.input_lines);

    if (result.IsError())
        goto cleanup;

    result = conversion.SetStandardCoefficient(params.standard_coefficient);

    if (result.IsError())
        goto cleanup;

    conversion.padding = params.padding;
    conversion.alpha = params.alpha;

cleanup:
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(result);

    LOG_DEBUG(
        Service_Y2R,
        "called input_format=%hhu output_format=%hhu rotation=%hhu block_alignment=%hhu "
        "input_line_width=%hu input_lines=%hu standard_coefficient=%hhu reserved=%hhu alpha=%hX",
        static_cast<u8>(params.input_format), static_cast<u8>(params.output_format),
        static_cast<u8>(params.rotation), static_cast<u8>(params.block_alignment),
        params.input_line_width, params.input_lines, static_cast<u8>(params.standard_coefficient),
        params.padding, params.alpha);
}

static void PingProcess(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x2A, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u8>(0);

    LOG_WARNING(Service_Y2R, "(STUBBED) called");
}

static void DriverInitialize(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x2B, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

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

    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_Y2R, "called");
}

static void DriverFinalize(Interface* self) {
    IPC::RequestParser rp(Kernel::GetCommandBuffer(), 0x2C, 0, 0);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

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
