// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <iomanip>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <SDL.h>
#include <inih/cpp/INIReader.h>
#include "citra/config.h"
#include "citra/default_ini.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/param_package.h"
#include "core/hle/service/service.h"
#include "core/settings.h"
#include "input_common/main.h"
#include "input_common/udp/client.h"

Config::Config() {
    // TODO: Don't hardcode the path; let the frontend decide where to put the config files.
    sdl2_config_loc = FileUtil::GetUserPath(D_CONFIG_IDX) + "sdl2-config.ini";
    sdl2_config = std::make_unique<INIReader>(sdl2_config_loc);

    Reload();
}

Config::~Config() = default;

bool Config::LoadINI(const std::string& default_contents, bool retry) {
    const char* location = this->sdl2_config_loc.c_str();
    if (sdl2_config->ParseError() < 0) {
        if (retry) {
            LOG_WARNING(Config, "Failed to load {}. Creating file from defaults...", location);
            FileUtil::CreateFullPath(location);
            FileUtil::WriteStringToFile(true, default_contents, location);
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
    SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_Z, SDL_SCANCODE_X, SDL_SCANCODE_T,
    SDL_SCANCODE_G, SDL_SCANCODE_F, SDL_SCANCODE_H, SDL_SCANCODE_Q, SDL_SCANCODE_W,
    SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_1, SDL_SCANCODE_2, SDL_SCANCODE_B,
};

static const std::array<std::array<int, 5>, Settings::NativeAnalog::NumAnalogs> default_analogs{{
    {
        SDL_SCANCODE_UP,
        SDL_SCANCODE_DOWN,
        SDL_SCANCODE_LEFT,
        SDL_SCANCODE_RIGHT,
        SDL_SCANCODE_D,
    },
    {
        SDL_SCANCODE_I,
        SDL_SCANCODE_K,
        SDL_SCANCODE_J,
        SDL_SCANCODE_L,
        SDL_SCANCODE_D,
    },
}};

void Config::ReadValues() {
    // Controls
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        Settings::values.buttons[i] =
            sdl2_config->Get("Controls", Settings::NativeButton::mapping[i], default_param);
        if (Settings::values.buttons[i].empty())
            Settings::values.buttons[i] = default_param;
    }

    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_analogs[i][4], 0.5f);
        Settings::values.analogs[i] =
            sdl2_config->Get("Controls", Settings::NativeAnalog::mapping[i], default_param);
        if (Settings::values.analogs[i].empty())
            Settings::values.analogs[i] = default_param;
    }

    Settings::values.motion_device =
        sdl2_config->Get("Controls", "motion_device",
                         "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0");
    Settings::values.touch_device =
        sdl2_config->Get("Controls", "touch_device", "engine:emu_window");
    Settings::values.udp_input_address =
        sdl2_config->Get("Controls", "udp_input_address", InputCommon::CemuhookUDP::DEFAULT_ADDR);
    Settings::values.udp_input_port = static_cast<u16>(sdl2_config->GetInteger(
        "Controls", "udp_input_port", InputCommon::CemuhookUDP::DEFAULT_PORT));

    // Core
    Settings::values.use_cpu_jit = sdl2_config->GetBoolean("Core", "use_cpu_jit", true);

    // Renderer
    Settings::values.use_hw_renderer = sdl2_config->GetBoolean("Renderer", "use_hw_renderer", true);
#ifdef __APPLE__
    // Hardware shader is broken on macos thanks to poor drivers.
    // We still want to provide this option for test/development purposes, but disable it by
    // default.
    Settings::values.use_hw_shader = sdl2_config->GetBoolean("Renderer", "use_hw_shader", false);
#else
    Settings::values.use_hw_shader = sdl2_config->GetBoolean("Renderer", "use_hw_shader", true);
#endif
    Settings::values.shaders_accurate_gs =
        sdl2_config->GetBoolean("Renderer", "shaders_accurate_gs", true);
    Settings::values.shaders_accurate_mul =
        sdl2_config->GetBoolean("Renderer", "shaders_accurate_mul", false);
    Settings::values.use_shader_jit = sdl2_config->GetBoolean("Renderer", "use_shader_jit", true);
    Settings::values.resolution_factor =
        static_cast<u16>(sdl2_config->GetInteger("Renderer", "resolution_factor", 1));
    Settings::values.use_vsync = sdl2_config->GetBoolean("Renderer", "use_vsync", false);
    Settings::values.use_frame_limit = sdl2_config->GetBoolean("Renderer", "use_frame_limit", true);
    Settings::values.frame_limit =
        static_cast<u16>(sdl2_config->GetInteger("Renderer", "frame_limit", 100));

    Settings::values.toggle_3d = sdl2_config->GetBoolean("Renderer", "toggle_3d", false);
    Settings::values.factor_3d =
        static_cast<u8>(sdl2_config->GetInteger("Renderer", "factor_3d", 0));

    Settings::values.bg_red = (float)sdl2_config->GetReal("Renderer", "bg_red", 0.0);
    Settings::values.bg_green = (float)sdl2_config->GetReal("Renderer", "bg_green", 0.0);
    Settings::values.bg_blue = (float)sdl2_config->GetReal("Renderer", "bg_blue", 0.0);

    // Layout
    Settings::values.layout_option =
        static_cast<Settings::LayoutOption>(sdl2_config->GetInteger("Layout", "layout_option", 0));
    Settings::values.swap_screen = sdl2_config->GetBoolean("Layout", "swap_screen", false);
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

    // Audio
    Settings::values.sink_id = sdl2_config->Get("Audio", "output_engine", "auto");
    Settings::values.enable_audio_stretching =
        sdl2_config->GetBoolean("Audio", "enable_audio_stretching", true);
    Settings::values.audio_device_id = sdl2_config->Get("Audio", "output_device", "auto");
    Settings::values.volume = sdl2_config->GetReal("Audio", "volume", 1);

    // Data Storage
    Settings::values.use_virtual_sd =
        sdl2_config->GetBoolean("Data Storage", "use_virtual_sd", true);

    // System
    Settings::values.is_new_3ds = sdl2_config->GetBoolean("System", "is_new_3ds", false);
    Settings::values.region_value =
        sdl2_config->GetInteger("System", "region_value", Settings::REGION_VALUE_AUTO_SELECT);
    Settings::values.init_clock =
        static_cast<Settings::InitClock>(sdl2_config->GetInteger("System", "init_clock", 1));
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
            sdl2_config->Get("System", "init_time", "2000-01-01 00:00:01"));
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
        sdl2_config->Get("Camera", "camera_outer_right_name", "blank");
    Settings::values.camera_config[OuterRightCamera] =
        sdl2_config->Get("Camera", "camera_outer_right_config", "");
    Settings::values.camera_flip[OuterRightCamera] =
        sdl2_config->GetInteger("Camera", "camera_outer_right_flip", 0);
    Settings::values.camera_name[InnerCamera] =
        sdl2_config->Get("Camera", "camera_inner_name", "blank");
    Settings::values.camera_config[InnerCamera] =
        sdl2_config->Get("Camera", "camera_inner_config", "");
    Settings::values.camera_flip[InnerCamera] =
        sdl2_config->GetInteger("Camera", "camera_inner_flip", 0);
    Settings::values.camera_name[OuterLeftCamera] =
        sdl2_config->Get("Camera", "camera_outer_left_name", "blank");
    Settings::values.camera_config[OuterLeftCamera] =
        sdl2_config->Get("Camera", "camera_outer_left_config", "");
    Settings::values.camera_flip[OuterLeftCamera] =
        sdl2_config->GetInteger("Camera", "camera_outer_left_flip", 0);

    // Miscellaneous
    Settings::values.log_filter = sdl2_config->Get("Miscellaneous", "log_filter", "*:Info");

    // Debugging
    Settings::values.use_gdbstub = sdl2_config->GetBoolean("Debugging", "use_gdbstub", false);
    Settings::values.gdbstub_port =
        static_cast<u16>(sdl2_config->GetInteger("Debugging", "gdbstub_port", 24689));

    for (const auto& service_module : Service::service_module_map) {
        bool use_lle = sdl2_config->GetBoolean("Debugging", "LLE\\" + service_module.name, false);
        Settings::values.lle_modules.emplace(service_module.name, use_lle);
    }

    // Web Service
    Settings::values.enable_telemetry =
        sdl2_config->GetBoolean("WebService", "enable_telemetry", true);
    Settings::values.telemetry_endpoint_url = sdl2_config->Get(
        "WebService", "telemetry_endpoint_url", "https://services.citra-emu.org/api/telemetry");
    Settings::values.verify_endpoint_url = sdl2_config->Get(
        "WebService", "verify_endpoint_url", "https://services.citra-emu.org/api/profile");
    Settings::values.announce_multiplayer_room_endpoint_url =
        sdl2_config->Get("WebService", "announce_multiplayer_room_endpoint_url",
                         "https://services.citra-emu.org/api/multiplayer/rooms");
    Settings::values.citra_username = sdl2_config->Get("WebService", "citra_username", "");
    Settings::values.citra_token = sdl2_config->Get("WebService", "citra_token", "");
}

void Config::Reload() {
    LoadINI(DefaultINI::sdl2_config_file);
    ReadValues();
}
