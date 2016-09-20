// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string>
#include <inih/cpp/INIReader.h>

class Config {
    std::unique_ptr<INIReader> sdl2_config;
    std::string sdl2_config_loc;

    bool LoadINI(const std::string& default_contents = "", bool retry = true);
    void ReadValues();

public:
    Config();

    void Reload();
};
