// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <QSettings>

#include "common/common_types.h"

class Config {
    QSettings* qt_config;
    std::string qt_config_loc;

    void ReadControls();
    void SaveControls();
    void ReadCore();
    void SaveCore();
    void ReadData();
    void SaveData();

    void ReadMiscellaneous();
    void SaveMiscellaneous();
public:
    Config();
    ~Config();

    void Reload();
    void Save();
};
