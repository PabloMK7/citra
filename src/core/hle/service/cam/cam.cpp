// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <future>
#include <memory>
#include <vector>
#include "common/bit_set.h"
#include "common/logging/log.h"
#include "core/core_timing.h"
#include "core/frontend/camera/factory.h"
#include "core/hle/kernel/event.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cam/cam_c.h"
#include "core/hle/service/cam/cam_q.h"
#include "core/hle/service/cam/cam_s.h"
#include "core/hle/service/cam/cam_u.h"
#include "core/hle/service/service.h"
#include "core/settings.h"

namespace Service {
namespace CAM {

namespace {

struct ContextConfig {
    Flip flip;
    Effect effect;
    OutputFormat format;
    Resolution resolution;
};

struct CameraConfig {
    std::unique_ptr<Camera::CameraInterface> impl;
    std::array<ContextConfig, 2> contexts;
    int current_context;
    FrameRate frame_rate;
};

struct PortConfig {
    int camera_id;

    bool is_active;            // set when the port is activated by an Activate call.
    bool is_pending_receiving; // set if SetReceiving is called when is_busy = false. When
                               // StartCapture is called then, this will trigger a receiving
                               // process and reset itself.
    bool is_busy;      // set when StartCapture is called and reset when StopCapture is called.
    bool is_receiving; // set when there is an ongoing receiving process.

    bool is_trimming;
    u16 x0; // x-coordinate of starting position for trimming
    u16 y0; // y-coordinate of starting position for trimming
    u16 x1; // x-coordinate of ending position for trimming
    u16 y1; // y-coordinate of ending position for trimming

    u32 transfer_bytes;

    Kernel::SharedPtr<Kernel::Event> completion_event;
    Kernel::SharedPtr<Kernel::Event> buffer_error_interrupt_event;
    Kernel::SharedPtr<Kernel::Event> vsync_interrupt_event;

    std::future<std::vector<u16>> capture_result; // will hold the received frame.
    VAddr dest;                                   // the destination address of a receiving process
    u32 dest_size;                                // the destination size of a receiving process

    void Clear() {
        completion_event->Clear();
        buffer_error_interrupt_event->Clear();
        vsync_interrupt_event->Clear();
        is_receiving = false;
        is_active = false;
        is_pending_receiving = false;
        is_busy = false;
        is_trimming = false;
        x0 = 0;
        y0 = 0;
        x1 = 0;
        y1 = 0;
        transfer_bytes = 256;
    }
};

// built-in resolution parameters
constexpr std::array<Resolution, 8> PRESET_RESOLUTION{{
    {640, 480, 0, 0, 639, 479},  // VGA
    {320, 240, 0, 0, 639, 479},  // QVGA
    {160, 120, 0, 0, 639, 479},  // QQVGA
    {352, 288, 26, 0, 613, 479}, // CIF
    {176, 144, 26, 0, 613, 479}, // QCIF
    {256, 192, 0, 0, 639, 479},  // DS_LCD
    {512, 384, 0, 0, 639, 479},  // DS_LCDx4
    {400, 240, 0, 48, 639, 431}, // CTR_TOP_LCD
}};

// latency in ms for each frame rate option
constexpr std::array<int, 13> LATENCY_BY_FRAME_RATE{{
    67,  // Rate_15
    67,  // Rate_15_To_5
    67,  // Rate_15_To_2
    100, // Rate_10
    118, // Rate_8_5
    200, // Rate_5
    50,  // Rate_20
    50,  // Rate_20_To_5
    33,  // Rate_30
    33,  // Rate_30_To_5
    67,  // Rate_15_To_10
    50,  // Rate_20_To_10
    33,  // Rate_30_To_10
}};

std::array<CameraConfig, NumCameras> cameras;
std::array<PortConfig, 2> ports;
int completion_event_callback;

const ResultCode ERROR_INVALID_ENUM_VALUE(ErrorDescription::InvalidEnumValue, ErrorModule::CAM,
                                          ErrorSummary::InvalidArgument, ErrorLevel::Usage);
const ResultCode ERROR_OUT_OF_RANGE(ErrorDescription::OutOfRange, ErrorModule::CAM,
                                    ErrorSummary::InvalidArgument, ErrorLevel::Usage);

void CompletionEventCallBack(u64 port_id, int) {
    PortConfig& port = ports[port_id];
    const CameraConfig& camera = cameras[port.camera_id];
    const auto buffer = port.capture_result.get();

    if (port.is_trimming) {
        u32 trim_width;
        u32 trim_height;
        const int original_width = camera.contexts[camera.current_context].resolution.width;
        const int original_height = camera.contexts[camera.current_context].resolution.height;
        if (port.x1 <= port.x0 || port.y1 <= port.y0 || port.x1 > original_width ||
            port.y1 > original_height) {
            LOG_ERROR(Service_CAM, "Invalid trimming coordinates x0=%u, y0=%u, x1=%u, y1=%u",
                      port.x0, port.y0, port.x1, port.y1);
            trim_width = 0;
            trim_height = 0;
        } else {
            trim_width = port.x1 - port.x0;
            trim_height = port.y1 - port.y0;
        }

        u32 trim_size = (port.x1 - port.x0) * (port.y1 - port.y0) * 2;
        if (port.dest_size != trim_size) {
            LOG_ERROR(Service_CAM, "The destination size (%u) doesn't match the source (%u)!",
                      port.dest_size, trim_size);
        }

        const u32 src_offset = port.y0 * original_width + port.x0;
        const u16* src_ptr = buffer.data() + src_offset;
        // Note: src_size_left is int because it can be negative if the buffer size doesn't match.
        int src_size_left = static_cast<int>((buffer.size() - src_offset) * sizeof(u16));
        VAddr dest_ptr = port.dest;
        // Note: dest_size_left and line_bytes are int to match the type of src_size_left.
        int dest_size_left = static_cast<int>(port.dest_size);
        const int line_bytes = static_cast<int>(trim_width * sizeof(u16));

        for (u32 y = 0; y < trim_height; ++y) {
            int copy_length = std::min({line_bytes, dest_size_left, src_size_left});
            if (copy_length <= 0) {
                break;
            }
            Memory::WriteBlock(dest_ptr, src_ptr, copy_length);
            dest_ptr += copy_length;
            dest_size_left -= copy_length;
            src_ptr += original_width;
            src_size_left -= original_width * sizeof(u16);
        }
    } else {
        std::size_t buffer_size = buffer.size() * sizeof(u16);
        if (port.dest_size != buffer_size) {
            LOG_ERROR(Service_CAM, "The destination size (%u) doesn't match the source (%zu)!",
                      port.dest_size, buffer_size);
        }
        Memory::WriteBlock(port.dest, buffer.data(), std::min<u32>(port.dest_size, buffer_size));
    }

    port.is_receiving = false;
    port.completion_event->Signal();
}

// Starts a receiving process on the specified port. This can only be called when is_busy = true and
// is_receiving = false.
void StartReceiving(int port_id) {
    PortConfig& port = ports[port_id];
    port.is_receiving = true;

    // launches a capture task asynchronously
    const CameraConfig& camera = cameras[port.camera_id];
    port.capture_result =
        std::async(std::launch::async, &Camera::CameraInterface::ReceiveFrame, camera.impl.get());

    // schedules a completion event according to the frame rate. The event will block on the
    // capture task if it is not finished within the expected time
    CoreTiming::ScheduleEvent(
        msToCycles(LATENCY_BY_FRAME_RATE[static_cast<int>(camera.frame_rate)]),
        completion_event_callback, port_id);
}

// Cancels any ongoing receiving processes at the specified port. This is used by functions that
// stop capturing.
// TODO: what is the exact behaviour on real 3DS when stopping capture during an ongoing process?
//       Will the completion event still be signaled?
void CancelReceiving(int port_id) {
    if (!ports[port_id].is_receiving)
        return;
    LOG_WARNING(Service_CAM, "tries to cancel an ongoing receiving process.");
    CoreTiming::UnscheduleEvent(completion_event_callback, port_id);
    ports[port_id].capture_result.wait();
    ports[port_id].is_receiving = false;
}

// Activates the specified port with the specfied camera.
static void ActivatePort(int port_id, int camera_id) {
    if (ports[port_id].is_busy && ports[port_id].camera_id != camera_id) {
        CancelReceiving(port_id);
        cameras[ports[port_id].camera_id].impl->StopCapture();
        ports[port_id].is_busy = false;
    }
    ports[port_id].is_active = true;
    ports[port_id].camera_id = camera_id;
}

template <int max_index>
class CommandParamBitSet : public BitSet8 {
public:
    explicit CommandParamBitSet(u32 command_param)
        : BitSet8(static_cast<u8>(command_param & 0xFF)) {}

    bool IsValid() const {
        return m_val < (1 << max_index);
    }

    bool IsSingle() const {
        return IsValid() && Count() == 1;
    }
};

using PortSet = CommandParamBitSet<2>;
using ContextSet = CommandParamBitSet<2>;
using CameraSet = CommandParamBitSet<3>;

} // namespace

void StartCapture(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);

    if (port_select.IsValid()) {
        for (int i : port_select) {
            if (!ports[i].is_busy) {
                if (!ports[i].is_active) {
                    // This doesn't return an error, but seems to put the camera in an undefined
                    // state
                    LOG_ERROR(Service_CAM, "port %u hasn't been activated", i);
                } else {
                    cameras[ports[i].camera_id].impl->StartCapture();
                    ports[i].is_busy = true;
                    if (ports[i].is_pending_receiving) {
                        ports[i].is_pending_receiving = false;
                        StartReceiving(i);
                    }
                }
            } else {
                LOG_WARNING(Service_CAM, "port %u already started", i);
            }
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x1, 1, 0);

    LOG_DEBUG(Service_CAM, "called, port_select=%u", port_select.m_val);
}

void StopCapture(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);

    if (port_select.IsValid()) {
        for (int i : port_select) {
            if (ports[i].is_busy) {
                CancelReceiving(i);
                cameras[ports[i].camera_id].impl->StopCapture();
                ports[i].is_busy = false;
            } else {
                LOG_WARNING(Service_CAM, "port %u already stopped", i);
            }
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x2, 1, 0);

    LOG_DEBUG(Service_CAM, "called, port_select=%u", port_select.m_val);
}

void IsBusy(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);

    if (port_select.IsValid()) {
        bool is_busy = true;
        // Note: the behaviour on no or both ports selected are verified against real 3DS.
        for (int i : port_select) {
            is_busy &= ports[i].is_busy;
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
        cmd_buff[2] = is_busy ? 1 : 0;
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x3, 2, 0);

    LOG_DEBUG(Service_CAM, "called, port_select=%u", port_select.m_val);
}

void ClearBuffer(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);

    cmd_buff[0] = IPC::MakeHeader(0x4, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, port_select=%u", port_select.m_val);
}

void GetVsyncInterruptEvent(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);

    if (port_select.IsSingle()) {
        int port = *port_select.begin();
        cmd_buff[1] = RESULT_SUCCESS.raw;
        cmd_buff[2] = IPC::CopyHandleDesc();
        cmd_buff[3] = Kernel::g_handle_table.Create(ports[port].vsync_interrupt_event).MoveFrom();
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
        cmd_buff[2] = IPC::CopyHandleDesc();
        cmd_buff[2] = 0;
    }

    cmd_buff[0] = IPC::MakeHeader(0x5, 1, 2);

    LOG_WARNING(Service_CAM, "(STUBBED) called, port_select=%u", port_select.m_val);
}

void GetBufferErrorInterruptEvent(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);

    if (port_select.IsSingle()) {
        int port = *port_select.begin();
        cmd_buff[1] = RESULT_SUCCESS.raw;
        cmd_buff[2] = IPC::CopyHandleDesc();
        cmd_buff[3] =
            Kernel::g_handle_table.Create(ports[port].buffer_error_interrupt_event).MoveFrom();
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
        cmd_buff[2] = IPC::CopyHandleDesc();
        cmd_buff[2] = 0;
    }

    LOG_WARNING(Service_CAM, "(STUBBED) called, port_select=%u", port_select.m_val);
}

void SetReceiving(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const VAddr dest = cmd_buff[1];
    const PortSet port_select(cmd_buff[2]);
    const u32 image_size = cmd_buff[3];
    const u32 trans_unit = cmd_buff[4] & 0xFFFF;

    if (port_select.IsSingle()) {
        int port_id = *port_select.begin();
        PortConfig& port = ports[port_id];
        CancelReceiving(port_id);
        port.completion_event->Clear();
        port.dest = dest;
        port.dest_size = image_size;

        if (port.is_busy) {
            StartReceiving(port_id);
        } else {
            port.is_pending_receiving = true;
        }

        cmd_buff[1] = RESULT_SUCCESS.raw;
        cmd_buff[2] = IPC::CopyHandleDesc();
        cmd_buff[3] = Kernel::g_handle_table.Create(port.completion_event).MoveFrom();
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x7, 1, 2);

    LOG_DEBUG(Service_CAM, "called, addr=0x%X, port_select=%u, image_size=%u, trans_unit=%u", dest,
              port_select.m_val, image_size, trans_unit);
}

void IsFinishedReceiving(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);

    if (port_select.IsSingle()) {
        int port = *port_select.begin();
        cmd_buff[1] = RESULT_SUCCESS.raw;
        cmd_buff[2] = (ports[port].is_receiving || ports[port].is_pending_receiving) ? 0 : 1;
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x8, 2, 0);

    LOG_DEBUG(Service_CAM, "called, port_select=%u", port_select.m_val);
}

void SetTransferLines(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);
    const u32 transfer_lines = cmd_buff[2] & 0xFFFF;
    const u32 width = cmd_buff[3] & 0xFFFF;
    const u32 height = cmd_buff[4] & 0xFFFF;

    if (port_select.IsValid()) {
        for (int i : port_select) {
            ports[i].transfer_bytes = transfer_lines * width * 2;
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x9, 1, 0);

    LOG_WARNING(Service_CAM, "(STUBBED) called, port_select=%u, lines=%u, width=%u, height=%u",
                port_select.m_val, transfer_lines, width, height);
}

void GetMaxLines(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const u32 width = cmd_buff[1] & 0xFFFF;
    const u32 height = cmd_buff[2] & 0xFFFF;

    // Note: the result of the algorithm below are hwtested with width < 640 and with height < 480
    constexpr u32 MIN_TRANSFER_UNIT = 256;
    constexpr u32 MAX_BUFFER_SIZE = 2560;
    if (width * height * 2 % MIN_TRANSFER_UNIT != 0) {
        cmd_buff[1] = ERROR_OUT_OF_RANGE.raw;
    } else {
        u32 lines = MAX_BUFFER_SIZE / width;
        if (lines > height) {
            lines = height;
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
        while (height % lines != 0 || (lines * width * 2 % MIN_TRANSFER_UNIT != 0)) {
            --lines;
            if (lines == 0) {
                cmd_buff[1] = ERROR_OUT_OF_RANGE.raw;
                break;
            }
        }
        cmd_buff[2] = lines;
    }

    cmd_buff[0] = IPC::MakeHeader(0xA, 2, 0);

    LOG_DEBUG(Service_CAM, "called, width=%u, height=%u", width, height);
}

void SetTransferBytes(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);
    const u32 transfer_bytes = cmd_buff[2] & 0xFFFF;
    const u32 width = cmd_buff[3] & 0xFFFF;
    const u32 height = cmd_buff[4] & 0xFFFF;

    if (port_select.IsValid()) {
        for (int i : port_select) {
            ports[i].transfer_bytes = transfer_bytes;
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0xB, 1, 0);

    LOG_WARNING(Service_CAM, "(STUBBED)called, port_select=%u, bytes=%u, width=%u, height=%u",
                port_select.m_val, transfer_bytes, width, height);
}

void GetTransferBytes(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);

    if (port_select.IsSingle()) {
        int port = *port_select.begin();
        cmd_buff[1] = RESULT_SUCCESS.raw;
        cmd_buff[2] = ports[port].transfer_bytes;
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0xC, 2, 0);

    LOG_WARNING(Service_CAM, "(STUBBED)called, port_select=%u", port_select.m_val);
}

void GetMaxBytes(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const u32 width = cmd_buff[1] & 0xFFFF;
    const u32 height = cmd_buff[2] & 0xFFFF;

    // Note: the result of the algorithm below are hwtested with width < 640 and with height < 480
    constexpr u32 MIN_TRANSFER_UNIT = 256;
    constexpr u32 MAX_BUFFER_SIZE = 2560;
    if (width * height * 2 % MIN_TRANSFER_UNIT != 0) {
        cmd_buff[1] = ERROR_OUT_OF_RANGE.raw;
    } else {
        u32 bytes = MAX_BUFFER_SIZE;

        while (width * height * 2 % bytes != 0) {
            bytes -= MIN_TRANSFER_UNIT;
        }

        cmd_buff[1] = RESULT_SUCCESS.raw;
        cmd_buff[2] = bytes;
    }
    cmd_buff[0] = IPC::MakeHeader(0xD, 2, 0);

    LOG_DEBUG(Service_CAM, "called, width=%u, height=%u", width, height);
}

void SetTrimming(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);
    const bool trim = (cmd_buff[2] & 0xFF) != 0;

    if (port_select.IsValid()) {
        for (int i : port_select) {
            ports[i].is_trimming = trim;
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0xE, 1, 0);

    LOG_DEBUG(Service_CAM, "called, port_select=%u, trim=%d", port_select.m_val, trim);
}

void IsTrimming(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);

    if (port_select.IsSingle()) {
        int port = *port_select.begin();
        cmd_buff[1] = RESULT_SUCCESS.raw;
        cmd_buff[2] = ports[port].is_trimming;
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0xF, 2, 0);

    LOG_DEBUG(Service_CAM, "called, port_select=%u", port_select.m_val);
}

void SetTrimmingParams(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);
    const u16 x0 = static_cast<u16>(cmd_buff[2] & 0xFFFF);
    const u16 y0 = static_cast<u16>(cmd_buff[3] & 0xFFFF);
    const u16 x1 = static_cast<u16>(cmd_buff[4] & 0xFFFF);
    const u16 y1 = static_cast<u16>(cmd_buff[5] & 0xFFFF);

    if (port_select.IsValid()) {
        for (int i : port_select) {
            ports[i].x0 = x0;
            ports[i].y0 = y0;
            ports[i].x1 = x1;
            ports[i].y1 = y1;
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x10, 1, 0);

    LOG_DEBUG(Service_CAM, "called, port_select=%u, x0=%u, y0=%u, x1=%u, y1=%u", port_select.m_val,
              x0, y0, x1, y1);
}

void GetTrimmingParams(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);

    if (port_select.IsSingle()) {
        int port = *port_select.begin();
        cmd_buff[1] = RESULT_SUCCESS.raw;
        cmd_buff[2] = ports[port].x0;
        cmd_buff[3] = ports[port].y0;
        cmd_buff[4] = ports[port].x1;
        cmd_buff[5] = ports[port].y1;
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x11, 5, 0);

    LOG_DEBUG(Service_CAM, "called, port_select=%u", port_select.m_val);
}

void SetTrimmingParamsCenter(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const PortSet port_select(cmd_buff[1]);
    const u16 trim_w = static_cast<u16>(cmd_buff[2] & 0xFFFF);
    const u16 trim_h = static_cast<u16>(cmd_buff[3] & 0xFFFF);
    const u16 cam_w = static_cast<u16>(cmd_buff[4] & 0xFFFF);
    const u16 cam_h = static_cast<u16>(cmd_buff[5] & 0xFFFF);

    if (port_select.IsValid()) {
        for (int i : port_select) {
            ports[i].x0 = (cam_w - trim_w) / 2;
            ports[i].y0 = (cam_h - trim_h) / 2;
            ports[i].x1 = ports[i].x0 + trim_w;
            ports[i].y1 = ports[i].y0 + trim_h;
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select=%u", port_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x12, 1, 0);

    LOG_DEBUG(Service_CAM, "called, port_select=%u, trim_w=%u, trim_h=%u, cam_w=%u, cam_h=%u",
              port_select.m_val, trim_w, trim_h, cam_w, cam_h);
}

void Activate(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const CameraSet camera_select(cmd_buff[1]);

    if (camera_select.IsValid()) {
        if (camera_select.m_val == 0) { // deactive all
            for (int i = 0; i < 2; ++i) {
                if (ports[i].is_busy) {
                    CancelReceiving(i);
                    cameras[ports[i].camera_id].impl->StopCapture();
                    ports[i].is_busy = false;
                }
                ports[i].is_active = false;
            }
            cmd_buff[1] = RESULT_SUCCESS.raw;
        } else if (camera_select[0] && camera_select[1]) {
            LOG_ERROR(Service_CAM, "camera 0 and 1 can't be both activated");
            cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
        } else {
            if (camera_select[0]) {
                ActivatePort(0, 0);
            } else if (camera_select[1]) {
                ActivatePort(0, 1);
            }

            if (camera_select[2]) {
                ActivatePort(1, 2);
            }
            cmd_buff[1] = RESULT_SUCCESS.raw;
        }
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select=%u", camera_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x13, 1, 0);

    LOG_DEBUG(Service_CAM, "called, camera_select=%u", camera_select.m_val);
}

void SwitchContext(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const CameraSet camera_select(cmd_buff[1]);
    const ContextSet context_select(cmd_buff[2]);

    if (camera_select.IsValid() && context_select.IsSingle()) {
        int context = *context_select.begin();
        for (int camera : camera_select) {
            cameras[camera].current_context = context;
            const ContextConfig& context_config = cameras[camera].contexts[context];
            cameras[camera].impl->SetFlip(context_config.flip);
            cameras[camera].impl->SetEffect(context_config.effect);
            cameras[camera].impl->SetFormat(context_config.format);
            cameras[camera].impl->SetResolution(context_config.resolution);
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select=%u, context_select=%u", camera_select.m_val,
                  context_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x14, 1, 0);

    LOG_DEBUG(Service_CAM, "called, camera_select=%u, context_select=%u", camera_select.m_val,
              context_select.m_val);
}

void FlipImage(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const CameraSet camera_select(cmd_buff[1]);
    const Flip flip = static_cast<Flip>(cmd_buff[2] & 0xFF);
    const ContextSet context_select(cmd_buff[3]);

    if (camera_select.IsValid() && context_select.IsValid()) {
        for (int camera : camera_select) {
            for (int context : context_select) {
                cameras[camera].contexts[context].flip = flip;
                if (cameras[camera].current_context == context) {
                    cameras[camera].impl->SetFlip(flip);
                }
            }
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select=%u, context_select=%u", camera_select.m_val,
                  context_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x1D, 1, 0);

    LOG_DEBUG(Service_CAM, "called, camera_select=%u, flip=%d, context_select=%u",
              camera_select.m_val, static_cast<int>(flip), context_select.m_val);
}

void SetDetailSize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const CameraSet camera_select(cmd_buff[1]);
    Resolution resolution;
    resolution.width = static_cast<u16>(cmd_buff[2] & 0xFFFF);
    resolution.height = static_cast<u16>(cmd_buff[3] & 0xFFFF);
    resolution.crop_x0 = static_cast<u16>(cmd_buff[4] & 0xFFFF);
    resolution.crop_y0 = static_cast<u16>(cmd_buff[5] & 0xFFFF);
    resolution.crop_x1 = static_cast<u16>(cmd_buff[6] & 0xFFFF);
    resolution.crop_y1 = static_cast<u16>(cmd_buff[7] & 0xFFFF);
    const ContextSet context_select(cmd_buff[8]);

    if (camera_select.IsValid() && context_select.IsValid()) {
        for (int camera : camera_select) {
            for (int context : context_select) {
                cameras[camera].contexts[context].resolution = resolution;
                if (cameras[camera].current_context == context) {
                    cameras[camera].impl->SetResolution(resolution);
                }
            }
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select=%u, context_select=%u", camera_select.m_val,
                  context_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x1E, 1, 0);

    LOG_DEBUG(Service_CAM, "called, camera_select=%u, width=%u, height=%u, crop_x0=%u, crop_y0=%u, "
                           "crop_x1=%u, crop_y1=%u, context_select=%u",
              camera_select.m_val, resolution.width, resolution.height, resolution.crop_x0,
              resolution.crop_y0, resolution.crop_x1, resolution.crop_y1, context_select.m_val);
}

void SetSize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const CameraSet camera_select(cmd_buff[1]);
    const u32 size = cmd_buff[2] & 0xFF;
    const ContextSet context_select(cmd_buff[3]);

    if (camera_select.IsValid() && context_select.IsValid()) {
        for (int camera : camera_select) {
            for (int context : context_select) {
                cameras[camera].contexts[context].resolution = PRESET_RESOLUTION[size];
                if (cameras[camera].current_context == context) {
                    cameras[camera].impl->SetResolution(PRESET_RESOLUTION[size]);
                }
            }
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select=%u, context_select=%u", camera_select.m_val,
                  context_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x1F, 1, 0);

    LOG_DEBUG(Service_CAM, "called, camera_select=%u, size=%u, context_select=%u",
              camera_select.m_val, size, context_select.m_val);
}

void SetFrameRate(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const CameraSet camera_select(cmd_buff[1]);
    const FrameRate frame_rate = static_cast<FrameRate>(cmd_buff[2] & 0xFF);

    if (camera_select.IsValid()) {
        for (int camera : camera_select) {
            cameras[camera].frame_rate = frame_rate;
            // TODO(wwylele): consider hinting the actual camera with the expected frame rate
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select=%u", camera_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x20, 1, 0);

    LOG_WARNING(Service_CAM, "(STUBBED) called, camera_select=%u, frame_rate=%d",
                camera_select.m_val, static_cast<int>(frame_rate));
}

void SetEffect(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const CameraSet camera_select(cmd_buff[1]);
    const Effect effect = static_cast<Effect>(cmd_buff[2] & 0xFF);
    const ContextSet context_select(cmd_buff[3]);

    if (camera_select.IsValid() && context_select.IsValid()) {
        for (int camera : camera_select) {
            for (int context : context_select) {
                cameras[camera].contexts[context].effect = effect;
                if (cameras[camera].current_context == context) {
                    cameras[camera].impl->SetEffect(effect);
                }
            }
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select=%u, context_select=%u", camera_select.m_val,
                  context_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x22, 1, 0);

    LOG_DEBUG(Service_CAM, "called, camera_select=%u, effect=%d, context_select=%u",
              camera_select.m_val, static_cast<int>(effect), context_select.m_val);
}

void SetOutputFormat(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const CameraSet camera_select(cmd_buff[1]);
    const OutputFormat format = static_cast<OutputFormat>(cmd_buff[2] & 0xFF);
    const ContextSet context_select(cmd_buff[3]);

    if (camera_select.IsValid() && context_select.IsValid()) {
        for (int camera : camera_select) {
            for (int context : context_select) {
                cameras[camera].contexts[context].format = format;
                if (cameras[camera].current_context == context) {
                    cameras[camera].impl->SetFormat(format);
                }
            }
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select=%u, context_select=%u", camera_select.m_val,
                  context_select.m_val);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(0x25, 1, 0);

    LOG_DEBUG(Service_CAM, "called, camera_select=%u, format=%d, context_select=%u",
              camera_select.m_val, static_cast<int>(format), context_select.m_val);
}

void SynchronizeVsyncTiming(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    const u32 camera_select1 = cmd_buff[1] & 0xFF;
    const u32 camera_select2 = cmd_buff[2] & 0xFF;

    cmd_buff[0] = IPC::MakeHeader(0x29, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called, camera_select1=%u, camera_select2=%u",
                camera_select1, camera_select2);
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

void SetPackageParameterWithoutContext(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    PackageParameterWithoutContext package;
    std::memcpy(&package, cmd_buff + 1, sizeof(package));

    cmd_buff[0] = IPC::MakeHeader(0x33, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_WARNING(Service_CAM, "(STUBBED) called");
}

template <typename PackageParameterType, int command_id>
static void SetPackageParameter() {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    PackageParameterType package;
    std::memcpy(&package, cmd_buff + 1, sizeof(package));

    const CameraSet camera_select(static_cast<u32>(package.camera_select));
    const ContextSet context_select(static_cast<u32>(package.context_select));

    if (camera_select.IsValid() && context_select.IsValid()) {
        for (int camera_id : camera_select) {
            CameraConfig& camera = cameras[camera_id];
            for (int context_id : context_select) {
                ContextConfig& context = camera.contexts[context_id];
                context.effect = package.effect;
                context.flip = package.flip;
                context.resolution = package.GetResolution();
                if (context_id == camera.current_context) {
                    camera.impl->SetEffect(context.effect);
                    camera.impl->SetFlip(context.flip);
                    camera.impl->SetResolution(context.resolution);
                }
            }
        }
        cmd_buff[1] = RESULT_SUCCESS.raw;
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select=%u, context_select=%u", package.camera_select,
                  package.context_select);
        cmd_buff[1] = ERROR_INVALID_ENUM_VALUE.raw;
    }

    cmd_buff[0] = IPC::MakeHeader(command_id, 1, 0);

    LOG_DEBUG(Service_CAM, "called");
}

Resolution PackageParameterWithContext::GetResolution() {
    return PRESET_RESOLUTION[static_cast<int>(size)];
}

void SetPackageParameterWithContext(Service::Interface* self) {
    SetPackageParameter<PackageParameterWithContext, 0x34>();
}

void SetPackageParameterWithContextDetail(Service::Interface* self) {
    SetPackageParameter<PackageParameterWithContextDetail, 0x35>();
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

    for (int camera_id = 0; camera_id < NumCameras; ++camera_id) {
        CameraConfig& camera = cameras[camera_id];
        camera.current_context = 0;
        for (int context_id = 0; context_id < 2; ++context_id) {
            // Note: the following default values are verified against real 3DS
            ContextConfig& context = camera.contexts[context_id];
            context.flip = camera_id == 1 ? Flip::Horizontal : Flip::None;
            context.effect = Effect::None;
            context.format = OutputFormat::YUV422;
            context.resolution =
                context_id == 0 ? PRESET_RESOLUTION[5 /*DS_LCD*/] : PRESET_RESOLUTION[0 /*VGA*/];
        }
        camera.impl = Camera::CreateCamera(Settings::values.camera_name[camera_id],
                                           Settings::values.camera_config[camera_id]);
        camera.impl->SetFlip(camera.contexts[0].flip);
        camera.impl->SetEffect(camera.contexts[0].effect);
        camera.impl->SetFormat(camera.contexts[0].format);
        camera.impl->SetResolution(camera.contexts[0].resolution);
    }

    for (PortConfig& port : ports) {
        port.Clear();
    }

    cmd_buff[0] = IPC::MakeHeader(0x39, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_CAM, "called");
}

void DriverFinalize(Service::Interface* self) {
    u32* cmd_buff = Kernel::GetCommandBuffer();

    CancelReceiving(0);
    CancelReceiving(1);

    for (CameraConfig& camera : cameras) {
        camera.impl = nullptr;
    }

    cmd_buff[0] = IPC::MakeHeader(0x3A, 1, 0);
    cmd_buff[1] = RESULT_SUCCESS.raw;

    LOG_DEBUG(Service_CAM, "called");
}

void Init() {
    using namespace Kernel;

    AddService(new CAM_C_Interface);
    AddService(new CAM_Q_Interface);
    AddService(new CAM_S_Interface);
    AddService(new CAM_U_Interface);

    for (PortConfig& port : ports) {
        port.completion_event = Event::Create(ResetType::Sticky, "CAM_U::completion_event");
        port.buffer_error_interrupt_event =
            Event::Create(ResetType::OneShot, "CAM_U::buffer_error_interrupt_event");
        port.vsync_interrupt_event =
            Event::Create(ResetType::OneShot, "CAM_U::vsync_interrupt_event");
    }
    completion_event_callback =
        CoreTiming::RegisterEvent("CAM_U::CompletionEventCallBack", CompletionEventCallBack);
}

void Shutdown() {
    CancelReceiving(0);
    CancelReceiving(1);
    for (PortConfig& port : ports) {
        port.completion_event = nullptr;
        port.buffer_error_interrupt_event = nullptr;
        port.vsync_interrupt_event = nullptr;
    }
    for (CameraConfig& camera : cameras) {
        camera.impl = nullptr;
    }
}

} // namespace CAM

} // namespace Service
