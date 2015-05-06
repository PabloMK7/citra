// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <GLFW/glfw3.h>

#include "citra/default_ini.h"

#include "common/file_util.h"
#include "common/logging/log.h"

#include "core/settings.h"
#include "core/core.h"

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

void Config::ReadValues() {
    // Controls
    Settings::values.pad_a_key = glfw_config->GetInteger("Controls", "pad_a", GLFW_KEY_A);
    Settings::values.pad_b_key = glfw_config->GetInteger("Controls", "pad_b", GLFW_KEY_S);
    Settings::values.pad_x_key = glfw_config->GetInteger("Controls", "pad_x", GLFW_KEY_Z);
    Settings::values.pad_y_key = glfw_config->GetInteger("Controls", "pad_y", GLFW_KEY_X);
    Settings::values.pad_l_key = glfw_config->GetInteger("Controls", "pad_l", GLFW_KEY_Q);
    Settings::values.pad_r_key = glfw_config->GetInteger("Controls", "pad_r", GLFW_KEY_W);
    Settings::values.pad_zl_key = glfw_config->GetInteger("Controls", "pad_zl", GLFW_KEY_1);
    Settings::values.pad_zr_key = glfw_config->GetInteger("Controls", "pad_zr", GLFW_KEY_2);
    Settings::values.pad_start_key  = glfw_config->GetInteger("Controls", "pad_start",  GLFW_KEY_M);
    Settings::values.pad_select_key = glfw_config->GetInteger("Controls", "pad_select", GLFW_KEY_N);
    Settings::values.pad_home_key   = glfw_config->GetInteger("Controls", "pad_home",   GLFW_KEY_B);
    Settings::values.pad_dup_key    = glfw_config->GetInteger("Controls", "pad_dup",    GLFW_KEY_T);
    Settings::values.pad_ddown_key  = glfw_config->GetInteger("Controls", "pad_ddown",  GLFW_KEY_G);
    Settings::values.pad_dleft_key  = glfw_config->GetInteger("Controls", "pad_dleft",  GLFW_KEY_F);
    Settings::values.pad_dright_key = glfw_config->GetInteger("Controls", "pad_dright", GLFW_KEY_H);
    Settings::values.pad_sup_key    = glfw_config->GetInteger("Controls", "pad_sup",    GLFW_KEY_UP);
    Settings::values.pad_sdown_key  = glfw_config->GetInteger("Controls", "pad_sdown",  GLFW_KEY_DOWN);
    Settings::values.pad_sleft_key  = glfw_config->GetInteger("Controls", "pad_sleft",  GLFW_KEY_LEFT);
    Settings::values.pad_sright_key = glfw_config->GetInteger("Controls", "pad_sright", GLFW_KEY_RIGHT);
    Settings::values.pad_cup_key    = glfw_config->GetInteger("Controls", "pad_cup",    GLFW_KEY_I);
    Settings::values.pad_cdown_key  = glfw_config->GetInteger("Controls", "pad_cdown",  GLFW_KEY_K);
    Settings::values.pad_cleft_key  = glfw_config->GetInteger("Controls", "pad_cleft",  GLFW_KEY_J);
    Settings::values.pad_cright_key = glfw_config->GetInteger("Controls", "pad_cright", GLFW_KEY_L);

    // Core
    Settings::values.gpu_refresh_rate = glfw_config->GetInteger("Core", "gpu_refresh_rate", 30);
    Settings::values.frame_skip = glfw_config->GetInteger("Core", "frame_skip", 0);

    // Renderer
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
