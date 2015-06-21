// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>

class QSettings;

class Config {
    QSettings* qt_config;
    std::string qt_config_loc;

    void ReadValues();
    void SaveValues();
public:
    Config();
    ~Config();

    void Reload();
    void Save();
};
