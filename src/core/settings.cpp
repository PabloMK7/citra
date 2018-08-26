// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/dsp_interface.h"
#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir.h"
#include "core/settings.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

namespace Settings {

Values values = {};

void Apply() {

    GDBStub::SetServerPort(values.gdbstub_port);
    GDBStub::ToggleServer(values.use_gdbstub);

    VideoCore::g_hw_renderer_enabled = values.use_hw_renderer;
    VideoCore::g_shader_jit_enabled = values.use_shader_jit;
    VideoCore::g_hw_shader_enabled = values.use_hw_shader;
    VideoCore::g_hw_shader_accurate_gs = values.shaders_accurate_gs;
    VideoCore::g_hw_shader_accurate_mul = values.shaders_accurate_mul;

    if (VideoCore::g_renderer) {
        VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
    }

    VideoCore::g_renderer_bg_color_update_requested = true;

    if (Core::System::GetInstance().IsPoweredOn()) {
        Core::DSP().SetSink(values.sink_id, values.audio_device_id);
        Core::DSP().EnableStretching(values.enable_audio_stretching);
    }

    Service::HID::ReloadInputDevices();
    Service::IR::ReloadInputDevices();
    Service::CAM::ReloadCameraDevices();
}

template <typename T>
void LogSetting(const std::string& name, const T& value) {
    LOG_INFO(Config, "{}: {}", name, value);
}

void LogSettings() {
    LOG_INFO(Config, "Citra Configuration:");
    LogSetting("Core_UseCpuJit", Settings::values.use_cpu_jit);
    LogSetting("Renderer_UseHwRenderer", Settings::values.use_hw_renderer);
    LogSetting("Renderer_UseHwShader", Settings::values.use_hw_shader);
    LogSetting("Renderer_ShadersAccurateGs", Settings::values.shaders_accurate_gs);
    LogSetting("Renderer_ShadersAccurateMul", Settings::values.shaders_accurate_mul);
    LogSetting("Renderer_UseShaderJit", Settings::values.use_shader_jit);
    LogSetting("Renderer_UseResolutionFactor", Settings::values.resolution_factor);
    LogSetting("Renderer_UseVsync", Settings::values.use_vsync);
    LogSetting("Renderer_UseFrameLimit", Settings::values.use_frame_limit);
    LogSetting("Renderer_FrameLimit", Settings::values.frame_limit);
    LogSetting("Layout_Toggle3d", Settings::values.toggle_3d);
    LogSetting("Layout_Factor3d", Settings::values.factor_3d);
    LogSetting("Layout_LayoutOption", static_cast<int>(Settings::values.layout_option));
    LogSetting("Layout_SwapScreen", Settings::values.swap_screen);
    LogSetting("Audio_OutputEngine", Settings::values.sink_id);
    LogSetting("Audio_EnableAudioStretching", Settings::values.enable_audio_stretching);
    LogSetting("Audio_OutputDevice", Settings::values.audio_device_id);
    using namespace Service::CAM;
    LogSetting("Camera_OuterRightName", Settings::values.camera_name[OuterRightCamera]);
    LogSetting("Camera_OuterRightConfig", Settings::values.camera_config[OuterRightCamera]);
    LogSetting("Camera_OuterRightFlip", Settings::values.camera_flip[OuterRightCamera]);
    LogSetting("Camera_InnerName", Settings::values.camera_name[InnerCamera]);
    LogSetting("Camera_InnerConfig", Settings::values.camera_config[InnerCamera]);
    LogSetting("Camera_InnerFlip", Settings::values.camera_flip[InnerCamera]);
    LogSetting("Camera_OuterLeftName", Settings::values.camera_name[OuterLeftCamera]);
    LogSetting("Camera_OuterLeftConfig", Settings::values.camera_config[OuterLeftCamera]);
    LogSetting("Camera_OuterLeftFlip", Settings::values.camera_flip[OuterLeftCamera]);
    LogSetting("DataStorage_UseVirtualSd", Settings::values.use_virtual_sd);
    LogSetting("System_IsNew3ds", Settings::values.is_new_3ds);
    LogSetting("System_RegionValue", Settings::values.region_value);
    LogSetting("Debugging_UseGdbstub", Settings::values.use_gdbstub);
    LogSetting("Debugging_GdbstubPort", Settings::values.gdbstub_port);
}

} // namespace Settings
