// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>
#include <inih/cpp/INIReader.h>

#include "citra/default_ini.h"

#include "common/file_util.h"
#include "common/logging/log.h"

#include "core/settings.h"

#include "config.h"

Config::Config() {
    // TODO: Don't hardcode the path; let the frontend decide where to put the config files.
    glfw_config_loc = FileUtil::GetUserPath(D_CONFIG_IDX) + "glfw-config.ini";
    glfw_config = new INIReader(glfw_config_loc);

    Reload();
}

bool Config::LoadINI(INIReader* config, const char* location, const std::string& default_contents, bool retry) {
    if (config->ParseError() < 0) {
        if (retry) {
            LOG_WARNING(Config, "Failed to load %s. Creating file from defaults...", location);
            FileUtil::CreateFullPath(location);
            FileUtil::WriteStringToFile(true, default_contents, location);
            *config = INIReader(location); // Reopen file

            return LoadINI(config, location, default_contents, false);
        }
        LOG_ERROR(Config, "Failed.");
        return false;
    }
    LOG_INFO(Config, "Successfully loaded %s", location);
    return true;
}

static const std::array<int, Settings::NativeInput::NUM_INPUTS> defaults = {
    GLFW_KEY_A, GLFW_KEY_S, GLFW_KEY_Z, GLFW_KEY_X,
    GLFW_KEY_Q, GLFW_KEY_W, GLFW_KEY_1, GLFW_KEY_2,
    GLFW_KEY_M, GLFW_KEY_N, GLFW_KEY_B,
    GLFW_KEY_T, GLFW_KEY_G, GLFW_KEY_F, GLFW_KEY_H,
    GLFW_KEY_UP, GLFW_KEY_DOWN, GLFW_KEY_LEFT, GLFW_KEY_RIGHT,
    GLFW_KEY_I, GLFW_KEY_K, GLFW_KEY_J, GLFW_KEY_L
};

void Config::ReadValues() {
    // Controls
    for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
        Settings::values.input_mappings[Settings::NativeInput::All[i]] =
            glfw_config->GetInteger("Controls", Settings::NativeInput::Mapping[i], defaults[i]);
    }

    // Core
    Settings::values.frame_skip = glfw_config->GetInteger("Core", "frame_skip", 0);

    // Renderer
    Settings::values.use_hw_renderer = glfw_config->GetBoolean("Renderer", "use_hw_renderer", false);

    Settings::values.bg_red   = (float)glfw_config->GetReal("Renderer", "bg_red",   1.0);
    Settings::values.bg_green = (float)glfw_config->GetReal("Renderer", "bg_green", 1.0);
    Settings::values.bg_blue  = (float)glfw_config->GetReal("Renderer", "bg_blue",  1.0);

    // Data Storage
    Settings::values.use_virtual_sd = glfw_config->GetBoolean("Data Storage", "use_virtual_sd", true);

    // System Region
    Settings::values.region_value = glfw_config->GetInteger("System Region", "region_value", 1);

    // Miscellaneous
    Settings::values.log_filter = glfw_config->Get("Miscellaneous", "log_filter", "*:Info");
}

void Config::Reload() {
    LoadINI(glfw_config, glfw_config_loc.c_str(), DefaultINI::glfw_config_file);
    ReadValues();
}

Config::~Config() {
    delete glfw_config;
}
