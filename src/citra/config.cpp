// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include <inih/cpp/INIReader.h>

#include <SDL.h>

#include "citra/default_ini.h"

#include "common/file_util.h"
#include "common/logging/log.h"

#include "core/settings.h"

#include "config.h"

Config::Config() {
    // TODO: Don't hardcode the path; let the frontend decide where to put the config files.
    sdl2_config_loc = FileUtil::GetUserPath(D_CONFIG_IDX) + "sdl2-config.ini";
    sdl2_config = std::make_unique<INIReader>(sdl2_config_loc);

    Reload();
}

bool Config::LoadINI(const std::string& default_contents, bool retry) {
    const char* location = this->sdl2_config_loc.c_str();
    if (sdl2_config->ParseError() < 0) {
        if (retry) {
            LOG_WARNING(Config, "Failed to load %s. Creating file from defaults...", location);
            FileUtil::CreateFullPath(location);
            FileUtil::WriteStringToFile(true, default_contents, location);
            sdl2_config = std::make_unique<INIReader>(location); // Reopen file

            return LoadINI(default_contents, false);
        }
        LOG_ERROR(Config, "Failed.");
        return false;
    }
    LOG_INFO(Config, "Successfully loaded %s", location);
    return true;
}

static const std::array<int, Settings::NativeInput::NUM_INPUTS> defaults = {
    // directly mapped keys
    SDL_SCANCODE_A, SDL_SCANCODE_S, SDL_SCANCODE_Z, SDL_SCANCODE_X,
    SDL_SCANCODE_Q, SDL_SCANCODE_W, SDL_SCANCODE_1, SDL_SCANCODE_2,
    SDL_SCANCODE_M, SDL_SCANCODE_N, SDL_SCANCODE_B,
    SDL_SCANCODE_T, SDL_SCANCODE_G, SDL_SCANCODE_F, SDL_SCANCODE_H,
    SDL_SCANCODE_I, SDL_SCANCODE_K, SDL_SCANCODE_J, SDL_SCANCODE_L,

    // indirectly mapped keys
    SDL_SCANCODE_UP, SDL_SCANCODE_DOWN, SDL_SCANCODE_LEFT, SDL_SCANCODE_RIGHT,
    SDL_SCANCODE_D,
};

void Config::ReadValues() {
    // Controls
    for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
        Settings::values.input_mappings[Settings::NativeInput::All[i]] =
            sdl2_config->GetInteger("Controls", Settings::NativeInput::Mapping[i], defaults[i]);
    }
    Settings::values.pad_circle_modifier_scale = (float)sdl2_config->GetReal("Controls", "pad_circle_modifier_scale", 0.5);

    // Core
    Settings::values.frame_skip = sdl2_config->GetInteger("Core", "frame_skip", 0);

    // Renderer
    Settings::values.use_hw_renderer = sdl2_config->GetBoolean("Renderer", "use_hw_renderer", true);
    Settings::values.use_shader_jit = sdl2_config->GetBoolean("Renderer", "use_shader_jit", true);
    Settings::values.use_scaled_resolution = sdl2_config->GetBoolean("Renderer", "use_scaled_resolution", false);
    Settings::values.use_vsync = sdl2_config->GetBoolean("Renderer", "use_vsync", false);

    Settings::values.bg_red   = (float)sdl2_config->GetReal("Renderer", "bg_red",   1.0);
    Settings::values.bg_green = (float)sdl2_config->GetReal("Renderer", "bg_green", 1.0);
    Settings::values.bg_blue  = (float)sdl2_config->GetReal("Renderer", "bg_blue",  1.0);

    // Audio
    Settings::values.sink_id = sdl2_config->Get("Audio", "output_engine", "auto");
    Settings::values.enable_audio_stretching = sdl2_config->GetBoolean("Audio", "enable_audio_stretching", true);

    // Data Storage
    Settings::values.use_virtual_sd = sdl2_config->GetBoolean("Data Storage", "use_virtual_sd", true);

    // System
    Settings::values.is_new_3ds = sdl2_config->GetBoolean("System", "is_new_3ds", false);
    Settings::values.region_value = sdl2_config->GetInteger("System", "region_value", 1);

    // Miscellaneous
    Settings::values.log_filter = sdl2_config->Get("Miscellaneous", "log_filter", "*:Info");

    // Debugging
    Settings::values.use_gdbstub = sdl2_config->GetBoolean("Debugging", "use_gdbstub", false);
    Settings::values.gdbstub_port = static_cast<u16>(sdl2_config->GetInteger("Debugging", "gdbstub_port", 24689));
}

void Config::Reload() {
    LoadINI(DefaultINI::sdl2_config_file);
    ReadValues();
}
