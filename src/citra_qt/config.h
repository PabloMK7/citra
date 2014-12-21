// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QSettings>

#include "common/common_types.h"

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
