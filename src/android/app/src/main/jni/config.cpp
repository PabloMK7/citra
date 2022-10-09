// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iomanip>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <inih/cpp/INIReader.h>

#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/param_package.h"
#include "core/core.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/service.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "input_common/udp/client.h"
#include "jni/camera/ndk_camera.h"
#include "jni/config.h"
#include "jni/default_ini.h"
#include "jni/input_manager.h"
#include "network/network_settings.h"

Config::Config() {
    // TODO: Don't hardcode the path; let the frontend decide where to put the config files.
    sdl2_config_loc = FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "config.ini";
    sdl2_config = std::make_unique<INIReader>(sdl2_config_loc);

    Reload();
}

Config::~Config() = default;

bool Config::LoadINI(const std::string& default_contents, bool retry) {
    const std::string& location = this->sdl2_config_loc;
    if (sdl2_config->ParseError() < 0) {
        if (retry) {
            LOG_WARNING(Config, "Failed to load {}. Creating file from defaults...", location);
            FileUtil::CreateFullPath(location);
            FileUtil::WriteStringToFile(true, location, default_contents);
            sdl2_config = std::make_unique<INIReader>(location); // Reopen file

            return LoadINI(default_contents, false);
        }
        LOG_ERROR(Config, "Failed.");
        return false;
    }
    LOG_INFO(Config, "Successfully loaded {}", location);
    return true;
}

static const std::array<int, Settings::NativeButton::NumButtons> default_buttons = {
    InputManager::N3DS_BUTTON_A,     InputManager::N3DS_BUTTON_B,
    InputManager::N3DS_BUTTON_X,     InputManager::N3DS_BUTTON_Y,
    InputManager::N3DS_DPAD_UP,      InputManager::N3DS_DPAD_DOWN,
    InputManager::N3DS_DPAD_LEFT,    InputManager::N3DS_DPAD_RIGHT,
    InputManager::N3DS_TRIGGER_L,    InputManager::N3DS_TRIGGER_R,
    InputManager::N3DS_BUTTON_START, InputManager::N3DS_BUTTON_SELECT,
    InputManager::N3DS_BUTTON_DEBUG, InputManager::N3DS_BUTTON_GPIO14,
    InputManager::N3DS_BUTTON_ZL,    InputManager::N3DS_BUTTON_ZR,
    InputManager::N3DS_BUTTON_HOME,
};

static const std::array<int, Settings::NativeAnalog::NumAnalogs> default_analogs{{
    InputManager::N3DS_CIRCLEPAD,
    InputManager::N3DS_STICK_C,
}};

void Config::UpdateCFG() {
    std::shared_ptr<Service::CFG::Module> cfg = std::make_shared<Service::CFG::Module>();
    cfg->SetSystemLanguage(static_cast<Service::CFG::SystemLanguage>(
        sdl2_config->GetInteger("System", "language", Service::CFG::SystemLanguage::LANGUAGE_EN)));
    cfg->UpdateConfigNANDSavegame();
}

void Config::ReadValues() {
    // Controls
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        std::string default_param = InputManager::GenerateButtonParamPackage(default_buttons[i]);
        Settings::values.current_input_profile.buttons[i] =
            sdl2_config->GetString("Controls", Settings::NativeButton::mapping[i], default_param);
        if (Settings::values.current_input_profile.buttons[i].empty())
            Settings::values.current_input_profile.buttons[i] = default_param;
    }

    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        std::string default_param = InputManager::GenerateAnalogParamPackage(default_analogs[i]);
        Settings::values.current_input_profile.analogs[i] =
            sdl2_config->GetString("Controls", Settings::NativeAnalog::mapping[i], default_param);
        if (Settings::values.current_input_profile.analogs[i].empty())
            Settings::values.current_input_profile.analogs[i] = default_param;
    }

    Settings::values.current_input_profile.motion_device = sdl2_config->GetString(
        "Controls", "motion_device",
        "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0");
    Settings::values.current_input_profile.touch_device =
        sdl2_config->GetString("Controls", "touch_device", "engine:emu_window");
    Settings::values.current_input_profile.udp_input_address = sdl2_config->GetString(
        "Controls", "udp_input_address", InputCommon::CemuhookUDP::DEFAULT_ADDR);
    Settings::values.current_input_profile.udp_input_port =
        static_cast<u16>(sdl2_config->GetInteger("Controls", "udp_input_port",
                                                 InputCommon::CemuhookUDP::DEFAULT_PORT));

    // Core
    Settings::values.use_cpu_jit = sdl2_config->GetBoolean("Core", "use_cpu_jit", true);
    Settings::values.cpu_clock_percentage =
        static_cast<int>(sdl2_config->GetInteger("Core", "cpu_clock_percentage", 100));

    // Premium
    Settings::values.texture_filter_name =
        sdl2_config->GetString("Premium", "texture_filter_name", "none");

    // Renderer
    Settings::values.use_gles = sdl2_config->GetBoolean("Renderer", "use_gles", true);
    Settings::values.use_hw_renderer = sdl2_config->GetBoolean("Renderer", "use_hw_renderer", true);
    Settings::values.use_hw_shader = sdl2_config->GetBoolean("Renderer", "use_hw_shader", true);
    Settings::values.shaders_accurate_mul =
        sdl2_config->GetBoolean("Renderer", "shaders_accurate_mul", false);
    Settings::values.use_shader_jit = sdl2_config->GetBoolean("Renderer", "use_shader_jit", true);
    Settings::values.resolution_factor =
        static_cast<u16>(sdl2_config->GetInteger("Renderer", "resolution_factor", 1));
    Settings::values.use_disk_shader_cache =
        sdl2_config->GetBoolean("Renderer", "use_disk_shader_cache", true);
    Settings::values.use_vsync_new = sdl2_config->GetBoolean("Renderer", "use_vsync_new", true);

    // Work around to map Android setting for enabling the frame limiter to the format Citra expects
    if (sdl2_config->GetBoolean("Renderer", "use_frame_limit", true)) {
        Settings::values.frame_limit =
            static_cast<u16>(sdl2_config->GetInteger("Renderer", "frame_limit", 100));
    } else {
        Settings::values.frame_limit = 0;
    }

    Settings::values.render_3d = static_cast<Settings::StereoRenderOption>(
        sdl2_config->GetInteger("Renderer", "render_3d", 0));
    Settings::values.factor_3d =
        static_cast<u8>(sdl2_config->GetInteger("Renderer", "factor_3d", 0));
    std::string default_shader = "none (builtin)";
    if (Settings::values.render_3d == Settings::StereoRenderOption::Anaglyph)
        default_shader = "dubois (builtin)";
    else if (Settings::values.render_3d == Settings::StereoRenderOption::Interlaced)
        default_shader = "horizontal (builtin)";
    Settings::values.pp_shader_name =
        sdl2_config->GetString("Renderer", "pp_shader_name", default_shader);
    Settings::values.filter_mode = sdl2_config->GetBoolean("Renderer", "filter_mode", true);

    Settings::values.bg_red = static_cast<float>(sdl2_config->GetReal("Renderer", "bg_red", 0.0));
    Settings::values.bg_green =
        static_cast<float>(sdl2_config->GetReal("Renderer", "bg_green", 0.0));
    Settings::values.bg_blue = static_cast<float>(sdl2_config->GetReal("Renderer", "bg_blue", 0.0));

    // Layout
    Settings::values.layout_option = static_cast<Settings::LayoutOption>(sdl2_config->GetInteger(
        "Layout", "layout_option", static_cast<int>(Settings::LayoutOption::MobileLandscape)));
    Settings::values.custom_layout = sdl2_config->GetBoolean("Layout", "custom_layout", false);
    Settings::values.custom_top_left =
        static_cast<u16>(sdl2_config->GetInteger("Layout", "custom_top_left", 0));
    Settings::values.custom_top_top =
        static_cast<u16>(sdl2_config->GetInteger("Layout", "custom_top_top", 0));
    Settings::values.custom_top_right =
        static_cast<u16>(sdl2_config->GetInteger("Layout", "custom_top_right", 400));
    Settings::values.custom_top_bottom =
        static_cast<u16>(sdl2_config->GetInteger("Layout", "custom_top_bottom", 240));
    Settings::values.custom_bottom_left =
        static_cast<u16>(sdl2_config->GetInteger("Layout", "custom_bottom_left", 40));
    Settings::values.custom_bottom_top =
        static_cast<u16>(sdl2_config->GetInteger("Layout", "custom_bottom_top", 240));
    Settings::values.custom_bottom_right =
        static_cast<u16>(sdl2_config->GetInteger("Layout", "custom_bottom_right", 360));
    Settings::values.custom_bottom_bottom =
        static_cast<u16>(sdl2_config->GetInteger("Layout", "custom_bottom_bottom", 480));
    Settings::values.cardboard_screen_size =
        static_cast<int>(sdl2_config->GetInteger("Layout", "cardboard_screen_size", 85));
    Settings::values.cardboard_x_shift =
        static_cast<int>(sdl2_config->GetInteger("Layout", "cardboard_x_shift", 0));
    Settings::values.cardboard_y_shift =
        static_cast<int>(sdl2_config->GetInteger("Layout", "cardboard_y_shift", 0));

    // Utility
    Settings::values.dump_textures = sdl2_config->GetBoolean("Utility", "dump_textures", false);
    Settings::values.custom_textures = sdl2_config->GetBoolean("Utility", "custom_textures", false);
    Settings::values.preload_textures =
        sdl2_config->GetBoolean("Utility", "preload_textures", false);

    // Audio
    Settings::values.enable_dsp_lle = sdl2_config->GetBoolean("Audio", "enable_dsp_lle", false);
    Settings::values.enable_dsp_lle_multithread =
        sdl2_config->GetBoolean("Audio", "enable_dsp_lle_multithread", false);
    Settings::values.sink_id = sdl2_config->GetString("Audio", "output_engine", "auto");
    Settings::values.enable_audio_stretching =
        sdl2_config->GetBoolean("Audio", "enable_audio_stretching", true);
    Settings::values.audio_device_id = sdl2_config->GetString("Audio", "output_device", "auto");
    Settings::values.volume = static_cast<float>(sdl2_config->GetReal("Audio", "volume", 1));
    Settings::values.mic_input_device =
        sdl2_config->GetString("Audio", "mic_input_device", "Default");
    Settings::values.mic_input_type =
        static_cast<Settings::MicInputType>(sdl2_config->GetInteger("Audio", "mic_input_type", 1));

    // Data Storage
    Settings::values.use_virtual_sd =
        sdl2_config->GetBoolean("Data Storage", "use_virtual_sd", true);

    // System
    Settings::values.is_new_3ds = sdl2_config->GetBoolean("System", "is_new_3ds", true);
    Settings::values.region_value =
        sdl2_config->GetInteger("System", "region_value", Settings::REGION_VALUE_AUTO_SELECT);
    Settings::values.init_clock =
        static_cast<Settings::InitClock>(sdl2_config->GetInteger("System", "init_clock", 0));
    {
        std::tm t;
        t.tm_sec = 1;
        t.tm_min = 0;
        t.tm_hour = 0;
        t.tm_mday = 1;
        t.tm_mon = 0;
        t.tm_year = 100;
        t.tm_isdst = 0;
        std::istringstream string_stream(
            sdl2_config->GetString("System", "init_time", "2000-01-01 00:00:01"));
        string_stream >> std::get_time(&t, "%Y-%m-%d %H:%M:%S");
        if (string_stream.fail()) {
            LOG_ERROR(Config, "Failed To parse init_time. Using 2000-01-01 00:00:01");
        }
        Settings::values.init_time =
            std::chrono::duration_cast<std::chrono::seconds>(
                std::chrono::system_clock::from_time_t(std::mktime(&t)).time_since_epoch())
                .count();
    }

    // Camera
    using namespace Service::CAM;
    Settings::values.camera_name[OuterRightCamera] =
        sdl2_config->GetString("Camera", "camera_outer_right_name", "ndk");
    Settings::values.camera_config[OuterRightCamera] = sdl2_config->GetString(
        "Camera", "camera_outer_right_config", std::string{Camera::NDK::BackCameraPlaceholder});
    Settings::values.camera_flip[OuterRightCamera] =
        sdl2_config->GetInteger("Camera", "camera_outer_right_flip", 0);
    Settings::values.camera_name[InnerCamera] =
        sdl2_config->GetString("Camera", "camera_inner_name", "ndk");
    Settings::values.camera_config[InnerCamera] = sdl2_config->GetString(
        "Camera", "camera_inner_config", std::string{Camera::NDK::FrontCameraPlaceholder});
    Settings::values.camera_flip[InnerCamera] =
        sdl2_config->GetInteger("Camera", "camera_inner_flip", 0);
    Settings::values.camera_name[OuterLeftCamera] =
        sdl2_config->GetString("Camera", "camera_outer_left_name", "ndk");
    Settings::values.camera_config[OuterLeftCamera] = sdl2_config->GetString(
        "Camera", "camera_outer_left_config", std::string{Camera::NDK::BackCameraPlaceholder});
    Settings::values.camera_flip[OuterLeftCamera] =
        sdl2_config->GetInteger("Camera", "camera_outer_left_flip", 0);

    // Miscellaneous
    Settings::values.log_filter = sdl2_config->GetString("Miscellaneous", "log_filter", "*:Info");

    // Debugging
    Settings::values.record_frame_times =
        sdl2_config->GetBoolean("Debugging", "record_frame_times", false);
    Settings::values.use_gdbstub = sdl2_config->GetBoolean("Debugging", "use_gdbstub", false);
    Settings::values.gdbstub_port =
        static_cast<u16>(sdl2_config->GetInteger("Debugging", "gdbstub_port", 24689));

    for (const auto& service_module : Service::service_module_map) {
        bool use_lle = sdl2_config->GetBoolean("Debugging", "LLE\\" + service_module.name, false);
        Settings::values.lle_modules.emplace(service_module.name, use_lle);
    }

    // Web Service
    NetSettings::values.enable_telemetry =
        sdl2_config->GetBoolean("WebService", "enable_telemetry", true);
    NetSettings::values.web_api_url =
        sdl2_config->GetString("WebService", "web_api_url", "https://api.citra-emu.org");
    NetSettings::values.citra_username = sdl2_config->GetString("WebService", "citra_username", "");
    NetSettings::values.citra_token = sdl2_config->GetString("WebService", "citra_token", "");

    // Update CFG file based on settings
    UpdateCFG();
}

void Config::Reload() {
    LoadINI(DefaultINI::sdl2_config_file);
    ReadValues();
}
