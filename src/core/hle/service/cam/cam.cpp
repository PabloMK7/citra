// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"

#include "core/hle/kernel/event.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cam/cam_c.h"
#include "core/hle/service/cam/cam_q.h"
#include "core/hle/service/cam/cam_s.h"
#include "core/hle/service/cam/cam_u.h"
#include "core/hle/service/service.h"

namespace Service {
namespace CAM {

static const u32 TRANSFER_BYTES = 5 * 1024;

static Kernel::SharedPtr<Kernel::Event> completion_event_cam1;
static Kernel::SharedPtr<Kernel::Event> completion_event_cam2;
static Kernel::SharedPtr<Kernel::Event> interrupt_error_event;
static Kernel::SharedPtr<Kernel::Event> vsync_interrupt_error_event;

void StartCapture(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x1, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%d", port);
}

void StopCapture(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x2, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%d", port);
}

void GetVsyncInterruptEvent(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x5, 1, 2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = IPC::CopyHandleDesc();
    cmd_buff[3] = Kernel::g_handle_table.Create(vsync_interrupt_error_event).MoveFrom();

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%d", port);
}

void GetBufferErrorInterruptEvent(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x6, 1, 2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = IPC::CopyHandleDesc();
    cmd_buff[3] = Kernel::g_handle_table.Create(interrupt_error_event).MoveFrom();

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%d", port);
}

void SetReceiving(Service::Interface* self) {
    u32* cmd_buff  = Kernel::GetCommandBuffer();

    VAddr dest     = cmd_buff[1];
    u8 port        = cmd_buff[2] & 0xFF;
    u32 image_size = cmd_buff[3];
    u16 trans_unit = cmd_buff[4] & 0xFFFF;

    Kernel::Event* completion_event = (Port)port == Port::Cam2 ?
            completion_event_cam2.get() : completion_event_cam1.get();

    completion_event->Signal();

    cmd_buff[0] = IPC::MakeHeader(0x7, 1, 2);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = IPC::CopyHandleDesc();
    cmd_buff[3] = Kernel::g_handle_table.Create(completion_event).MoveFrom();

    LOG_WARNING(Service_CAM, "(STUBBED) called, addr=0x%X, port=%d, image_size=%d, trans_unit=%d",
            dest, port, image_size, trans_unit);
}

void SetTransferLines(Service::Interface* self) {
    u32* cmd_buff      = Kernel::GetCommandBuffer();

    u8  port           = cmd_buff[1] & 0xFF;
    u16 transfer_lines = cmd_buff[2] & 0xFFFF;
    u16 width          = cmd_buff[3] & 0xFFFF;
    u16 height         = cmd_buff[4] & 0xFFFF;

    cmd_buff[0] = IPC::MakeHeader(0x9, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%d, lines=%d, width=%d, height=%d",
            port, transfer_lines, width, height);
}

void GetMaxLines(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u16 width  = cmd_buff[1] & 0xFFFF;
    u16 height = cmd_buff[2] & 0xFFFF;

    cmd_buff[0] = IPC::MakeHeader(0xA, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = TRANSFER_BYTES / (2 * width);

    LOG_WARNING(Service_CAM, "(STUBBED) called, width=%d, height=%d, lines = %d",
            width, height, cmd_buff[2]);
}

void GetTransferBytes(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 port = cmd_buff[1] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0xC, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = TRANSFER_BYTES;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%d", port);
}

void SetTrimming(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8   port = cmd_buff[1] & 0xFF;
    bool trim = (cmd_buff[2] & 0xFF) != 0;

    cmd_buff[0] = IPC::MakeHeader(0xE, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%d, trim=%d", port, trim);
}

void SetTrimmingParamsCenter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8  port  = cmd_buff[1] & 0xFF;
    s16 trimW = cmd_buff[2] & 0xFFFF;
    s16 trimH = cmd_buff[3] & 0xFFFF;
    s16 camW  = cmd_buff[4] & 0xFFFF;
    s16 camH  = cmd_buff[5] & 0xFFFF;

    cmd_buff[0] = IPC::MakeHeader(0x12, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port=%d, trimW=%d, trimH=%d, camW=%d, camH=%d",
            port, trimW, trimH, camW, camH);
}

void Activate(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 cam_select = cmd_buff[1] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x13, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, cam_select=%d",
            cam_select);
}

void FlipImage(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 cam_select = cmd_buff[1] & 0xFF;
    u8 flip       = cmd_buff[2] & 0xFF;
    u8 context    = cmd_buff[3] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x1D, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, cam_select=%d, flip=%d, context=%d",
            cam_select, flip, context);
}

void SetSize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 cam_select = cmd_buff[1] & 0xFF;
    u8 size       = cmd_buff[2] & 0xFF;
    u8 context    = cmd_buff[3] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x1F, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, cam_select=%d, size=%d, context=%d",
            cam_select, size, context);
}

void SetFrameRate(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 cam_select = cmd_buff[1] & 0xFF;
    u8 frame_rate = cmd_buff[2] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x20, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, cam_select=%d, frame_rate=%d",
            cam_select, frame_rate);
}

void GetStereoCameraCalibrationData(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    // Default values taken from yuriks' 3DS. Valid data is required here or games using the
    // calibration get stuck in an infinite CPU loop.
    StereoCameraCalibrationData data = {};
    data.isValidRotationXY = 0;
    data.scale = 1.001776f;
    data.rotationZ = 0.008322907f;
    data.translationX = -87.70484f;
    data.translationY = -7.640977f;
    data.rotationX = 0.0f;
    data.rotationY = 0.0f;
    data.angleOfViewRight = 64.66875f;
    data.angleOfViewLeft = 64.76067f;
    data.distanceToChart = 250.0f;
    data.distanceCameras = 35.0f;
    data.imageWidth = 640;
    data.imageHeight = 480;

    cmd_buff[0] = IPC::MakeHeader(0x2B, 17, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    memcpy(&cmd_buff[2], &data, sizeof(data));

    LOG_TRACE(Service_CAM, "called");
}

void GetSuitableY2rStandardCoefficient(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x36, 2, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;
    cmd_buff[2] = 0;

    LOG_WARNING(Service_CAM, "(STUBBED) called");
}

void PlayShutterSound(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    u8 sound_id = cmd_buff[1] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x38, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, sound_id=%d", sound_id);
}

void DriverInitialize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    completion_event_cam1->Clear();
    completion_event_cam2->Clear();
    interrupt_error_event->Clear();
    vsync_interrupt_error_event->Clear();

    cmd_buff[0] = IPC::MakeHeader(0x39, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called");
}

void DriverFinalize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    cmd_buff[0] = IPC::MakeHeader(0x3A, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called");
}

void Init() {
    using namespace Kernel;

    AddService(new CAM_C_Interface);
    AddService(new CAM_Q_Interface);
    AddService(new CAM_S_Interface);
    AddService(new CAM_U_Interface);

    completion_event_cam1 = Kernel::Event::Create(ResetType::OneShot, "CAM_U::completion_event_cam1");
    completion_event_cam2 = Kernel::Event::Create(ResetType::OneShot, "CAM_U::completion_event_cam2");
    interrupt_error_event = Kernel::Event::Create(ResetType::OneShot, "CAM_U::interrupt_error_event");
    vsync_interrupt_error_event = Kernel::Event::Create(ResetType::OneShot, "CAM_U::vsync_interrupt_error_event");
}

void Shutdown() {
    completion_event_cam1 = nullptr;
    completion_event_cam2 = nullptr;
    interrupt_error_event = nullptr;
    vsync_interrupt_error_event = nullptr;
}

} // namespace CAM

} // namespace Service
