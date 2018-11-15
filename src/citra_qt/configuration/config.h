// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <memory>
#include <string>
#include <QVariant>
#include "core/settings.h"

class QSettings;

class Config {
public:
    Config();
    ~Config();

    void Reload();
    void Save();

    static const std::array<int, Settings::NativeButton::NumButtons> default_buttons;
    static const std::array<std::array<int, 5>, Settings::NativeAnalog::NumAnalogs> default_analogs;

private:
    void ReadValues();
    void SaveValues();
    QVariant ReadSetting(const QString& name);
    QVariant ReadSetting(const QString& name, const QVariant& default_value);
    void WriteSetting(const QString& name, const QVariant& value);
    void WriteSetting(const QString& name, const QVariant& value, const QVariant& default_value);

    std::unique_ptr<QSettings> qt_config;
    std::string qt_config_loc;
};
