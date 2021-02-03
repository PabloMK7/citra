// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <string_view>
#include <utility>
#include "audio_core/dsp_interface.h"
#include "core/core.h"
#include "core/gdbstub/gdbstub.h"
#include "core/hle/kernel/shared_page.h"
#include "core/hle/service/cam/cam.h"
#include "core/hle/service/hid/hid.h"
#include "core/hle/service/ir/ir_rst.h"
#include "core/hle/service/ir/ir_user.h"
#include "core/hle/service/mic_u.h"
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
    VideoCore::g_separable_shader_enabled = values.separable_shader;
    VideoCore::g_hw_shader_accurate_mul = values.shaders_accurate_mul;
    VideoCore::g_use_disk_shader_cache = values.use_disk_shader_cache;

    if (VideoCore::g_renderer) {
        VideoCore::g_renderer->UpdateCurrentFramebufferLayout();
    }

    VideoCore::g_renderer_bg_color_update_requested = true;
    VideoCore::g_renderer_sampler_update_requested = true;
    VideoCore::g_renderer_shader_update_requested = true;
    VideoCore::g_texture_filter_update_requested = true;

    auto& system = Core::System::GetInstance();
    if (system.IsPoweredOn()) {
        system.CoreTiming().UpdateClockSpeed(values.cpu_clock_percentage);
        Core::DSP().SetSink(values.sink_id, values.audio_device_id);
        Core::DSP().EnableStretching(values.enable_audio_stretching);

        auto hid = Service::HID::GetModule(system);
        if (hid) {
            hid->ReloadInputDevices();
        }

        auto sm = system.ServiceManager();
        auto ir_user = sm.GetService<Service::IR::IR_USER>("ir:USER");
        if (ir_user)
            ir_user->ReloadInputDevices();
        auto ir_rst = sm.GetService<Service::IR::IR_RST>("ir:rst");
        if (ir_rst)
            ir_rst->ReloadInputDevices();

        auto cam = Service::CAM::GetModule(system);
        if (cam) {
            cam->ReloadCameraDevices();
        }

        Service::MIC::ReloadMic(system);
    }
}

void LogSettings() {
    const auto log_setting = [](std::string_view name, const auto& value) {
        LOG_INFO(Config, "{}: {}", name, value);
    };

    LOG_INFO(Config, "Citra Configuration:");
    log_setting("Core_UseCpuJit", values.use_cpu_jit);
    log_setting("Core_CPUClockPercentage", values.cpu_clock_percentage);
    log_setting("Renderer_UseGLES", values.use_gles);
    log_setting("Renderer_UseHwRenderer", values.use_hw_renderer);
    log_setting("Renderer_UseHwShader", values.use_hw_shader);
    log_setting("Renderer_SeparableShader", values.separable_shader);
    log_setting("Renderer_ShadersAccurateMul", values.shaders_accurate_mul);
    log_setting("Renderer_UseShaderJit", values.use_shader_jit);
    log_setting("Renderer_UseResolutionFactor", values.resolution_factor);
    log_setting("Renderer_FrameLimit", values.frame_limit);
    log_setting("Renderer_UseFrameLimitAlternate", values.use_frame_limit_alternate);
    log_setting("Renderer_FrameLimitAlternate", values.frame_limit_alternate);
    log_setting("Renderer_VSyncNew", values.use_vsync_new);
    log_setting("Renderer_PostProcessingShader", values.pp_shader_name);
    log_setting("Renderer_FilterMode", values.filter_mode);
    log_setting("Renderer_TextureFilterName", values.texture_filter_name);
    log_setting("Stereoscopy_Render3d", values.render_3d);
    log_setting("Stereoscopy_Factor3d", values.factor_3d);
    log_setting("Layout_LayoutOption", values.layout_option);
    log_setting("Layout_SwapScreen", values.swap_screen);
    log_setting("Layout_UprightScreen", values.upright_screen);
    log_setting("Utility_DumpTextures", values.dump_textures);
    log_setting("Utility_CustomTextures", values.custom_textures);
    log_setting("Utility_UseDiskShaderCache", values.use_disk_shader_cache);
    log_setting("Audio_EnableDspLle", values.enable_dsp_lle);
    log_setting("Audio_EnableDspLleMultithread", values.enable_dsp_lle_multithread);
    log_setting("Audio_OutputEngine", values.sink_id);
    log_setting("Audio_EnableAudioStretching", values.enable_audio_stretching);
    log_setting("Audio_OutputDevice", values.audio_device_id);
    log_setting("Audio_InputDeviceType", values.mic_input_type);
    log_setting("Audio_InputDevice", values.mic_input_device);
    using namespace Service::CAM;
    log_setting("Camera_OuterRightName", values.camera_name[OuterRightCamera]);
    log_setting("Camera_OuterRightConfig", values.camera_config[OuterRightCamera]);
    log_setting("Camera_OuterRightFlip", values.camera_flip[OuterRightCamera]);
    log_setting("Camera_InnerName", values.camera_name[InnerCamera]);
    log_setting("Camera_InnerConfig", values.camera_config[InnerCamera]);
    log_setting("Camera_InnerFlip", values.camera_flip[InnerCamera]);
    log_setting("Camera_OuterLeftName", values.camera_name[OuterLeftCamera]);
    log_setting("Camera_OuterLeftConfig", values.camera_config[OuterLeftCamera]);
    log_setting("Camera_OuterLeftFlip", values.camera_flip[OuterLeftCamera]);
    log_setting("DataStorage_UseVirtualSd", values.use_virtual_sd);
    log_setting("System_IsNew3ds", values.is_new_3ds);
    log_setting("System_RegionValue", values.region_value);
    log_setting("Debugging_UseGdbstub", values.use_gdbstub);
    log_setting("Debugging_GdbstubPort", values.gdbstub_port);
}

void LoadProfile(int index) {
    Settings::values.current_input_profile = Settings::values.input_profiles[index];
    Settings::values.current_input_profile_index = index;
}

void SaveProfile(int index) {
    Settings::values.input_profiles[index] = Settings::values.current_input_profile;
}

void CreateProfile(std::string name) {
    Settings::InputProfile profile = values.current_input_profile;
    profile.name = std::move(name);
    Settings::values.input_profiles.push_back(std::move(profile));
    Settings::values.current_input_profile_index =
        static_cast<int>(Settings::values.input_profiles.size()) - 1;
    Settings::LoadProfile(Settings::values.current_input_profile_index);
}

void DeleteProfile(int index) {
    Settings::values.input_profiles.erase(Settings::values.input_profiles.begin() + index);
    Settings::LoadProfile(0);
}

void RenameCurrentProfile(std::string new_name) {
    Settings::values.current_input_profile.name = std::move(new_name);
}

} // namespace Settings
