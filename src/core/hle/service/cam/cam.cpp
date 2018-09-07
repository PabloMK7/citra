// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include "common/bit_set.h"
#include "common/logging/log.h"
#include "core/core_timing.h"
#include "core/frontend/camera/factory.h"
#include "core/hle/ipc.h"
#include "core/hle/ipc_helpers.h"
#include "core/hle/kernel/event.h"
#include "core/hle/kernel/process.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/cam/cam_c.h"
#include "core/hle/service/cam/cam_q.h"
#include "core/hle/service/cam/cam_s.h"
#include "core/hle/service/cam/cam_u.h"
#include "core/memory.h"
#include "core/settings.h"

namespace Service {
namespace CAM {

static std::weak_ptr<Module> current_cam;

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

const ResultCode ERROR_INVALID_ENUM_VALUE(ErrorDescription::InvalidEnumValue, ErrorModule::CAM,
                                          ErrorSummary::InvalidArgument, ErrorLevel::Usage);
const ResultCode ERROR_OUT_OF_RANGE(ErrorDescription::OutOfRange, ErrorModule::CAM,
                                    ErrorSummary::InvalidArgument, ErrorLevel::Usage);

void Module::PortConfig::Clear() {
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

void Module::CompletionEventCallBack(u64 port_id, s64) {
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
            LOG_ERROR(Service_CAM, "Invalid trimming coordinates x0={}, y0={}, x1={}, y1={}",
                      port.x0, port.y0, port.x1, port.y1);
            trim_width = 0;
            trim_height = 0;
        } else {
            trim_width = port.x1 - port.x0;
            trim_height = port.y1 - port.y0;
        }

        u32 trim_size = (port.x1 - port.x0) * (port.y1 - port.y0) * 2;
        if (port.dest_size != trim_size) {
            LOG_ERROR(Service_CAM, "The destination size ({}) doesn't match the source ({})!",
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
            Memory::WriteBlock(*port.dest_process, dest_ptr, src_ptr, copy_length);
            dest_ptr += copy_length;
            dest_size_left -= copy_length;
            src_ptr += original_width;
            src_size_left -= original_width * sizeof(u16);
        }
    } else {
        std::size_t buffer_size = buffer.size() * sizeof(u16);
        if (port.dest_size != buffer_size) {
            LOG_ERROR(Service_CAM, "The destination size ({}) doesn't match the source ({})!",
                      port.dest_size, buffer_size);
        }
        Memory::WriteBlock(*port.dest_process, port.dest, buffer.data(),
                           std::min<std::size_t>(port.dest_size, buffer_size));
    }

    port.is_receiving = false;
    port.completion_event->Signal();
}

void Module::StartReceiving(int port_id) {
    PortConfig& port = ports[port_id];
    port.is_receiving = true;

    // launches a capture task asynchronously
    CameraConfig& camera = cameras[port.camera_id];
    port.capture_result = std::async(std::launch::async, [&camera, &port, this] {
        if (is_camera_reload_pending.exchange(false)) {
            // reinitialize the camera according to new settings
            camera.impl->StopCapture();
            LoadCameraImplementation(camera, port.camera_id);
            camera.impl->StartCapture();
        }
        return camera.impl->ReceiveFrame();
    });

    // schedules a completion event according to the frame rate. The event will block on the
    // capture task if it is not finished within the expected time
    CoreTiming::ScheduleEvent(
        msToCycles(LATENCY_BY_FRAME_RATE[static_cast<int>(camera.frame_rate)]),
        completion_event_callback, port_id);
}

void Module::CancelReceiving(int port_id) {
    if (!ports[port_id].is_receiving)
        return;
    LOG_WARNING(Service_CAM, "tries to cancel an ongoing receiving process.");
    CoreTiming::UnscheduleEvent(completion_event_callback, port_id);
    ports[port_id].capture_result.wait();
    ports[port_id].is_receiving = false;
}

void Module::ActivatePort(int port_id, int camera_id) {
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
    explicit CommandParamBitSet(u8 command_param) : BitSet8(command_param) {}

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

Module::Interface::Interface(std::shared_ptr<Module> cam, const char* name, u32 max_session)
    : ServiceFramework(name, max_session), cam(std::move(cam)) {}

Module::Interface::~Interface() = default;

void Module::Interface::StartCapture(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x01, 1, 0);
    const PortSet port_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (port_select.IsValid()) {
        for (int i : port_select) {
            if (!cam->ports[i].is_busy) {
                if (!cam->ports[i].is_active) {
                    // This doesn't return an error, but seems to put the camera in an undefined
                    // state
                    LOG_ERROR(Service_CAM, "port {} hasn't been activated", i);
                } else {
                    cam->cameras[cam->ports[i].camera_id].impl->StartCapture();
                    cam->ports[i].is_busy = true;
                    if (cam->ports[i].is_pending_receiving) {
                        cam->ports[i].is_pending_receiving = false;
                        cam->StartReceiving(i);
                    }
                }
            } else {
                LOG_WARNING(Service_CAM, "port {} already started", i);
            }
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_DEBUG(Service_CAM, "called, port_select={}", port_select.m_val);
}

void Module::Interface::StopCapture(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x02, 1, 0);
    const PortSet port_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    if (port_select.IsValid()) {
        for (int i : port_select) {
            if (cam->ports[i].is_busy) {
                cam->CancelReceiving(i);
                cam->cameras[cam->ports[i].camera_id].impl->StopCapture();
                cam->ports[i].is_busy = false;
            } else {
                LOG_WARNING(Service_CAM, "port {} already stopped", i);
            }
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_DEBUG(Service_CAM, "called, port_select={}", port_select.m_val);
}

void Module::Interface::IsBusy(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x03, 1, 0);
    const PortSet port_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    if (port_select.IsValid()) {
        bool is_busy = true;
        // Note: the behaviour on no or both ports selected are verified against real 3DS.
        for (int i : port_select) {
            is_busy &= cam->ports[i].is_busy;
        }
        rb.Push(RESULT_SUCCESS);
        rb.Push(is_busy);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
        rb.Skip(1, false);
    }

    LOG_DEBUG(Service_CAM, "called, port_select={}", port_select.m_val);
}

void Module::Interface::ClearBuffer(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x04, 1, 0);
    const PortSet port_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_CAM, "(STUBBED) called, port_select={}", port_select.m_val);
}

void Module::Interface::GetVsyncInterruptEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x05, 1, 0);
    const PortSet port_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    if (port_select.IsSingle()) {
        int port = *port_select.begin();
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(cam->ports[port].vsync_interrupt_event);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
        rb.PushCopyObjects<Kernel::Object>(nullptr);
    }

    LOG_WARNING(Service_CAM, "(STUBBED) called, port_select={}", port_select.m_val);
}

void Module::Interface::GetBufferErrorInterruptEvent(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x06, 1, 0);
    const PortSet port_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    if (port_select.IsSingle()) {
        int port = *port_select.begin();
        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(cam->ports[port].buffer_error_interrupt_event);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
        rb.PushCopyObjects<Kernel::Object>(nullptr);
    }

    LOG_WARNING(Service_CAM, "(STUBBED) called, port_select={}", port_select.m_val);
}

void Module::Interface::SetReceiving(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x07, 4, 2);
    const VAddr dest = rp.Pop<u32>();
    const PortSet port_select(rp.Pop<u8>());
    const u32 image_size = rp.Pop<u32>();
    const u16 trans_unit = rp.Pop<u16>();
    auto process = rp.PopObject<Kernel::Process>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 2);
    if (port_select.IsSingle()) {
        int port_id = *port_select.begin();
        PortConfig& port = cam->ports[port_id];
        cam->CancelReceiving(port_id);
        port.completion_event->Clear();
        port.dest_process = process.get();
        port.dest = dest;
        port.dest_size = image_size;

        if (port.is_busy) {
            cam->StartReceiving(port_id);
        } else {
            port.is_pending_receiving = true;
        }

        rb.Push(RESULT_SUCCESS);
        rb.PushCopyObjects(port.completion_event);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
        rb.PushCopyObjects<Kernel::Object>(nullptr);
    }

    LOG_DEBUG(Service_CAM, "called, addr=0x{:X}, port_select={}, image_size={}, trans_unit={}",
              dest, port_select.m_val, image_size, trans_unit);
}

void Module::Interface::IsFinishedReceiving(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x08, 1, 0);
    const PortSet port_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    if (port_select.IsSingle()) {
        int port = *port_select.begin();
        bool is_busy = cam->ports[port].is_receiving || cam->ports[port].is_pending_receiving;
        rb.Push(RESULT_SUCCESS);
        rb.Push(!is_busy);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
        rb.Skip(1, false);
    }

    LOG_DEBUG(Service_CAM, "called, port_select={}", port_select.m_val);
}

void Module::Interface::SetTransferLines(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x09, 4, 0);
    const PortSet port_select(rp.Pop<u8>());
    const u16 transfer_lines = rp.Pop<u16>();
    const u16 width = rp.Pop<u16>();
    const u16 height = rp.Pop<u16>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (port_select.IsValid()) {
        for (int i : port_select) {
            cam->ports[i].transfer_bytes = transfer_lines * width * 2;
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_WARNING(Service_CAM, "(STUBBED) called, port_select={}, lines={}, width={}, height={}",
                port_select.m_val, transfer_lines, width, height);
}

void Module::Interface::GetMaxLines(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0A, 2, 0);
    const u16 width = rp.Pop<u16>();
    const u16 height = rp.Pop<u16>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    // Note: the result of the algorithm below are hwtested with width < 640 and with height < 480
    constexpr u32 MIN_TRANSFER_UNIT = 256;
    constexpr u32 MAX_BUFFER_SIZE = 2560;
    if (width * height * 2 % MIN_TRANSFER_UNIT != 0) {
        rb.Push(ERROR_OUT_OF_RANGE);
        rb.Skip(1, false);
    } else {
        u32 lines = MAX_BUFFER_SIZE / width;
        if (lines > height) {
            lines = height;
        }
        ResultCode result = RESULT_SUCCESS;
        while (height % lines != 0 || (lines * width * 2 % MIN_TRANSFER_UNIT != 0)) {
            --lines;
            if (lines == 0) {
                result = ERROR_OUT_OF_RANGE;
                break;
            }
        }
        rb.Push(result);
        rb.Push(lines);
    }

    LOG_DEBUG(Service_CAM, "called, width={}, height={}", width, height);
}

void Module::Interface::SetTransferBytes(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0B, 4, 0);
    const PortSet port_select(rp.Pop<u8>());
    const u16 transfer_bytes = rp.Pop<u16>();
    const u16 width = rp.Pop<u16>();
    const u16 height = rp.Pop<u16>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (port_select.IsValid()) {
        for (int i : port_select) {
            cam->ports[i].transfer_bytes = transfer_bytes;
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_WARNING(Service_CAM, "(STUBBED)called, port_select={}, bytes={}, width={}, height={}",
                port_select.m_val, transfer_bytes, width, height);
}

void Module::Interface::GetTransferBytes(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0C, 1, 0);
    const PortSet port_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    if (port_select.IsSingle()) {
        int port = *port_select.begin();
        rb.Push(RESULT_SUCCESS);
        rb.Push(cam->ports[port].transfer_bytes);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
        rb.Skip(1, false);
    }

    LOG_WARNING(Service_CAM, "(STUBBED)called, port_select={}", port_select.m_val);
}

void Module::Interface::GetMaxBytes(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0D, 2, 0);
    const u16 width = rp.Pop<u16>();
    const u16 height = rp.Pop<u16>();

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);

    // Note: the result of the algorithm below are hwtested with width < 640 and with height < 480
    constexpr u32 MIN_TRANSFER_UNIT = 256;
    constexpr u32 MAX_BUFFER_SIZE = 2560;
    if (width * height * 2 % MIN_TRANSFER_UNIT != 0) {
        rb.Push(ERROR_OUT_OF_RANGE);
        rb.Skip(1, false);
    } else {
        u32 bytes = MAX_BUFFER_SIZE;

        while (width * height * 2 % bytes != 0) {
            bytes -= MIN_TRANSFER_UNIT;
        }

        rb.Push(RESULT_SUCCESS);
        rb.Push(bytes);
    }

    LOG_DEBUG(Service_CAM, "called, width={}, height={}", width, height);
}

void Module::Interface::SetTrimming(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0E, 2, 0);
    const PortSet port_select(rp.Pop<u8>());
    const bool trim = rp.Pop<bool>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (port_select.IsValid()) {
        for (int i : port_select) {
            cam->ports[i].is_trimming = trim;
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_DEBUG(Service_CAM, "called, port_select={}, trim={}", port_select.m_val, trim);
}

void Module::Interface::IsTrimming(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x0F, 1, 0);
    const PortSet port_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    if (port_select.IsSingle()) {
        int port = *port_select.begin();
        rb.Push(RESULT_SUCCESS);
        rb.Push(cam->ports[port].is_trimming);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
        rb.Skip(1, false);
    }

    LOG_DEBUG(Service_CAM, "called, port_select={}", port_select.m_val);
}

void Module::Interface::SetTrimmingParams(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x10, 5, 0);
    const PortSet port_select(rp.Pop<u8>());
    const u16 x0 = rp.Pop<u16>();
    const u16 y0 = rp.Pop<u16>();
    const u16 x1 = rp.Pop<u16>();
    const u16 y1 = rp.Pop<u16>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (port_select.IsValid()) {
        for (int i : port_select) {
            cam->ports[i].x0 = x0;
            cam->ports[i].y0 = y0;
            cam->ports[i].x1 = x1;
            cam->ports[i].y1 = y1;
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_DEBUG(Service_CAM, "called, port_select={}, x0={}, y0={}, x1={}, y1={}", port_select.m_val,
              x0, y0, x1, y1);
}

void Module::Interface::GetTrimmingParams(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x11, 1, 0);
    const PortSet port_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(5, 0);
    if (port_select.IsSingle()) {
        int port = *port_select.begin();
        rb.Push(RESULT_SUCCESS);
        rb.Push(cam->ports[port].x0);
        rb.Push(cam->ports[port].y0);
        rb.Push(cam->ports[port].x1);
        rb.Push(cam->ports[port].y1);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
        rb.Skip(4, false);
    }

    LOG_DEBUG(Service_CAM, "called, port_select={}", port_select.m_val);
}

void Module::Interface::SetTrimmingParamsCenter(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x12, 5, 0);
    const PortSet port_select(rp.Pop<u8>());
    const u16 trim_w = rp.Pop<u16>();
    const u16 trim_h = rp.Pop<u16>();
    const u16 cam_w = rp.Pop<u16>();
    const u16 cam_h = rp.Pop<u16>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (port_select.IsValid()) {
        for (int i : port_select) {
            cam->ports[i].x0 = (cam_w - trim_w) / 2;
            cam->ports[i].y0 = (cam_h - trim_h) / 2;
            cam->ports[i].x1 = cam->ports[i].x0 + trim_w;
            cam->ports[i].y1 = cam->ports[i].y0 + trim_h;
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid port_select={}", port_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_DEBUG(Service_CAM, "called, port_select={}, trim_w={}, trim_h={}, cam_w={}, cam_h={}",
              port_select.m_val, trim_w, trim_h, cam_w, cam_h);
}

void Module::Interface::Activate(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x13, 1, 0);
    const CameraSet camera_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (camera_select.IsValid()) {
        if (camera_select.m_val == 0) { // deactive all
            for (int i = 0; i < 2; ++i) {
                if (cam->ports[i].is_busy) {
                    cam->CancelReceiving(i);
                    cam->cameras[cam->ports[i].camera_id].impl->StopCapture();
                    cam->ports[i].is_busy = false;
                }
                cam->ports[i].is_active = false;
            }
            rb.Push(RESULT_SUCCESS);
        } else if (camera_select[0] && camera_select[1]) {
            LOG_ERROR(Service_CAM, "camera 0 and 1 can't be both activated");
            rb.Push(ERROR_INVALID_ENUM_VALUE);
        } else {
            if (camera_select[0]) {
                cam->ActivatePort(0, 0);
            } else if (camera_select[1]) {
                cam->ActivatePort(0, 1);
            }

            if (camera_select[2]) {
                cam->ActivatePort(1, 2);
            }
            rb.Push(RESULT_SUCCESS);
        }
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select={}", camera_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_DEBUG(Service_CAM, "called, camera_select={}", camera_select.m_val);
}

void Module::Interface::SwitchContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x14, 2, 0);
    const CameraSet camera_select(rp.Pop<u8>());
    const ContextSet context_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (camera_select.IsValid() && context_select.IsSingle()) {
        int context = *context_select.begin();
        for (int camera : camera_select) {
            cam->cameras[camera].current_context = context;
            const ContextConfig& context_config = cam->cameras[camera].contexts[context];
            cam->cameras[camera].impl->SetFlip(context_config.flip);
            cam->cameras[camera].impl->SetEffect(context_config.effect);
            cam->cameras[camera].impl->SetFormat(context_config.format);
            cam->cameras[camera].impl->SetResolution(context_config.resolution);
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select={}, context_select={}", camera_select.m_val,
                  context_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_DEBUG(Service_CAM, "called, camera_select={}, context_select={}", camera_select.m_val,
              context_select.m_val);
}

void Module::Interface::FlipImage(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1D, 3, 0);
    const CameraSet camera_select(rp.Pop<u8>());
    const Flip flip = static_cast<Flip>(rp.Pop<u8>());
    const ContextSet context_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (camera_select.IsValid() && context_select.IsValid()) {
        for (int camera : camera_select) {
            for (int context : context_select) {
                cam->cameras[camera].contexts[context].flip = flip;
                if (cam->cameras[camera].current_context == context) {
                    cam->cameras[camera].impl->SetFlip(flip);
                }
            }
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select={}, context_select={}", camera_select.m_val,
                  context_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_DEBUG(Service_CAM, "called, camera_select={}, flip={}, context_select={}",
              camera_select.m_val, static_cast<int>(flip), context_select.m_val);
}

void Module::Interface::SetDetailSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1E, 8, 0);
    const CameraSet camera_select(rp.Pop<u8>());
    Resolution resolution;
    resolution.width = rp.Pop<u16>();
    resolution.height = rp.Pop<u16>();
    resolution.crop_x0 = rp.Pop<u16>();
    resolution.crop_y0 = rp.Pop<u16>();
    resolution.crop_x1 = rp.Pop<u16>();
    resolution.crop_y1 = rp.Pop<u16>();
    const ContextSet context_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (camera_select.IsValid() && context_select.IsValid()) {
        for (int camera : camera_select) {
            for (int context : context_select) {
                cam->cameras[camera].contexts[context].resolution = resolution;
                if (cam->cameras[camera].current_context == context) {
                    cam->cameras[camera].impl->SetResolution(resolution);
                }
            }
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select={}, context_select={}", camera_select.m_val,
                  context_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_DEBUG(Service_CAM,
              "called, camera_select={}, width={}, height={}, crop_x0={}, crop_y0={}, "
              "crop_x1={}, crop_y1={}, context_select={}",
              camera_select.m_val, resolution.width, resolution.height, resolution.crop_x0,
              resolution.crop_y0, resolution.crop_x1, resolution.crop_y1, context_select.m_val);
}

void Module::Interface::SetSize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x1F, 3, 0);
    const CameraSet camera_select(rp.Pop<u8>());
    const u8 size = rp.Pop<u8>();
    const ContextSet context_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (camera_select.IsValid() && context_select.IsValid()) {
        for (int camera : camera_select) {
            for (int context : context_select) {
                cam->cameras[camera].contexts[context].resolution = PRESET_RESOLUTION[size];
                if (cam->cameras[camera].current_context == context) {
                    cam->cameras[camera].impl->SetResolution(PRESET_RESOLUTION[size]);
                }
            }
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select={}, context_select={}", camera_select.m_val,
                  context_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_DEBUG(Service_CAM, "called, camera_select={}, size={}, context_select={}",
              camera_select.m_val, size, context_select.m_val);
}

void Module::Interface::SetFrameRate(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x20, 2, 0);
    const CameraSet camera_select(rp.Pop<u8>());
    const FrameRate frame_rate = static_cast<FrameRate>(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (camera_select.IsValid()) {
        for (int camera : camera_select) {
            cam->cameras[camera].frame_rate = frame_rate;
            cam->cameras[camera].impl->SetFrameRate(frame_rate);
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select={}", camera_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_WARNING(Service_CAM, "(STUBBED) called, camera_select={}, frame_rate={}",
                camera_select.m_val, static_cast<int>(frame_rate));
}

void Module::Interface::SetEffect(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x22, 3, 0);
    const CameraSet camera_select(rp.Pop<u8>());
    const Effect effect = static_cast<Effect>(rp.Pop<u8>());
    const ContextSet context_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (camera_select.IsValid() && context_select.IsValid()) {
        for (int camera : camera_select) {
            for (int context : context_select) {
                cam->cameras[camera].contexts[context].effect = effect;
                if (cam->cameras[camera].current_context == context) {
                    cam->cameras[camera].impl->SetEffect(effect);
                }
            }
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select={}, context_select={}", camera_select.m_val,
                  context_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_DEBUG(Service_CAM, "called, camera_select={}, effect={}, context_select={}",
              camera_select.m_val, static_cast<int>(effect), context_select.m_val);
}

void Module::Interface::SetOutputFormat(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x25, 3, 0);
    const CameraSet camera_select(rp.Pop<u8>());
    const OutputFormat format = static_cast<OutputFormat>(rp.Pop<u8>());
    const ContextSet context_select(rp.Pop<u8>());

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    if (camera_select.IsValid() && context_select.IsValid()) {
        for (int camera : camera_select) {
            for (int context : context_select) {
                cam->cameras[camera].contexts[context].format = format;
                if (cam->cameras[camera].current_context == context) {
                    cam->cameras[camera].impl->SetFormat(format);
                }
            }
        }
        rb.Push(RESULT_SUCCESS);
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select={}, context_select={}", camera_select.m_val,
                  context_select.m_val);
        rb.Push(ERROR_INVALID_ENUM_VALUE);
    }

    LOG_DEBUG(Service_CAM, "called, camera_select={}, format={}, context_select={}",
              camera_select.m_val, static_cast<int>(format), context_select.m_val);
}

void Module::Interface::SynchronizeVsyncTiming(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x29, 2, 0);
    const u8 camera_select1 = rp.Pop<u8>();
    const u8 camera_select2 = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_CAM, "(STUBBED) called, camera_select1={}, camera_select2={}",
                camera_select1, camera_select2);
}

void Module::Interface::GetStereoCameraCalibrationData(Kernel::HLERequestContext& ctx) {
    IPC::RequestBuilder rb = IPC::RequestParser(ctx, 0x2B, 0, 0).MakeBuilder(17, 0);

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

    rb.Push(RESULT_SUCCESS);
    rb.PushRaw(data);

    LOG_TRACE(Service_CAM, "called");
}

void Module::Interface::SetPackageParameterWithoutContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x33, 11, 0);

    PackageParameterWithoutContext package;
    rp.PopRaw(package);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_CAM, "(STUBBED) called");
}

template <typename PackageParameterType>
ResultCode Module::SetPackageParameter(const PackageParameterType& package) {
    const CameraSet camera_select(package.camera_select);
    const ContextSet context_select(package.context_select);

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
        return RESULT_SUCCESS;
    } else {
        LOG_ERROR(Service_CAM, "invalid camera_select={}, context_select={}", package.camera_select,
                  package.context_select);
        return ERROR_INVALID_ENUM_VALUE;
    }
}

Resolution PackageParameterWithContext::GetResolution() const {
    return PRESET_RESOLUTION[static_cast<int>(size)];
}

void Module::Interface::SetPackageParameterWithContext(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x34, 5, 0);

    PackageParameterWithContext package;
    rp.PopRaw(package);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    ResultCode result = cam->SetPackageParameter(package);
    rb.Push(result);

    LOG_DEBUG(Service_CAM, "called");
}

void Module::Interface::SetPackageParameterWithContextDetail(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x35, 7, 0);

    PackageParameterWithContextDetail package;
    rp.PopRaw(package);

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    ResultCode result = cam->SetPackageParameter(package);
    rb.Push(result);

    LOG_DEBUG(Service_CAM, "called");
}

void Module::Interface::GetSuitableY2rStandardCoefficient(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x36, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(2, 0);
    rb.Push(RESULT_SUCCESS);
    rb.Push<u32>(0);

    LOG_WARNING(Service_CAM, "(STUBBED) called");
}

void Module::Interface::PlayShutterSound(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x38, 1, 0);
    u8 sound_id = rp.Pop<u8>();

    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);
    rb.Push(RESULT_SUCCESS);

    LOG_WARNING(Service_CAM, "(STUBBED) called, sound_id={}", sound_id);
}

void Module::Interface::DriverInitialize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x39, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    for (int camera_id = 0; camera_id < NumCameras; ++camera_id) {
        CameraConfig& camera = cam->cameras[camera_id];
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
        cam->LoadCameraImplementation(camera, camera_id);
    }

    for (PortConfig& port : cam->ports) {
        port.Clear();
    }

    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_CAM, "called");
}

void Module::Interface::DriverFinalize(Kernel::HLERequestContext& ctx) {
    IPC::RequestParser rp(ctx, 0x3A, 0, 0);
    IPC::RequestBuilder rb = rp.MakeBuilder(1, 0);

    cam->CancelReceiving(0);
    cam->CancelReceiving(1);

    for (CameraConfig& camera : cam->cameras) {
        camera.impl = nullptr;
    }

    rb.Push(RESULT_SUCCESS);

    LOG_DEBUG(Service_CAM, "called");
}

Module::Module() {
    using namespace Kernel;
    for (PortConfig& port : ports) {
        port.completion_event = Event::Create(ResetType::Sticky, "CAM::completion_event");
        port.buffer_error_interrupt_event =
            Event::Create(ResetType::OneShot, "CAM::buffer_error_interrupt_event");
        port.vsync_interrupt_event =
            Event::Create(ResetType::OneShot, "CAM::vsync_interrupt_event");
    }
    completion_event_callback = CoreTiming::RegisterEvent(
        "CAM::CompletionEventCallBack",
        [this](u64 userdata, s64 cycles_late) { CompletionEventCallBack(userdata, cycles_late); });
}

Module::~Module() {
    CancelReceiving(0);
    CancelReceiving(1);
}

void Module::ReloadCameraDevices() {
    is_camera_reload_pending.store(true);
}

void Module::LoadCameraImplementation(CameraConfig& camera, int camera_id) {
    camera.impl = Camera::CreateCamera(
        Settings::values.camera_name[camera_id], Settings::values.camera_config[camera_id],
        static_cast<Service::CAM::Flip>(Settings::values.camera_flip[camera_id]));
    camera.impl->SetFlip(camera.contexts[0].flip);
    camera.impl->SetEffect(camera.contexts[0].effect);
    camera.impl->SetFormat(camera.contexts[0].format);
    camera.impl->SetResolution(camera.contexts[0].resolution);
}

void ReloadCameraDevices() {
    if (auto cam = current_cam.lock())
        cam->ReloadCameraDevices();
}

void InstallInterfaces(SM::ServiceManager& service_manager) {
    auto cam = std::make_shared<Module>();
    current_cam = cam;

    std::make_shared<CAM_U>(cam)->InstallAsService(service_manager);
    std::make_shared<CAM_S>(cam)->InstallAsService(service_manager);
    std::make_shared<CAM_C>(cam)->InstallAsService(service_manager);
    std::make_shared<CAM_Q>()->InstallAsService(service_manager);
}

} // namespace CAM

} // namespace Service
