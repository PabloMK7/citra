// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>

#include <inih/cpp/INIReader.h>

#include "common/common_types.h"

class Config {
    INIReader* glfw_config;
    std::string glfw_config_loc;

    bool LoadINI(INIReader* config, const char* location, const std::string& default_contents="", bool retry=true);
    void ReadValues();
public:
    Config();
    ~Config();

    void Reload();
};
