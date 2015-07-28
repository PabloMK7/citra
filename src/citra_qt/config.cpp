// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QSettings>
#include <QString>
#include <QStringList>

#include "core/settings.h"
#include "common/file_util.h"

#include "config.h"

Config::Config() {

    // TODO: Don't hardcode the path; let the frontend decide where to put the config files.
    qt_config_loc = FileUtil::GetUserPath(D_CONFIG_IDX) + "qt-config.ini";
    FileUtil::CreateFullPath(qt_config_loc);
    qt_config = new QSettings(QString::fromStdString(qt_config_loc), QSettings::IniFormat);

    Reload();
}

static const std::array<QVariant, Settings::NativeInput::NUM_INPUTS> defaults = {
    Qt::Key_A, Qt::Key_S, Qt::Key_Z, Qt::Key_X,
    Qt::Key_Q, Qt::Key_W, Qt::Key_1, Qt::Key_2,
    Qt::Key_M, Qt::Key_N, Qt::Key_B,
    Qt::Key_T, Qt::Key_G, Qt::Key_F, Qt::Key_H,
    Qt::Key_Up, Qt::Key_Down, Qt::Key_Left, Qt::Key_Right,
    Qt::Key_I, Qt::Key_K, Qt::Key_J, Qt::Key_L
};

void Config::ReadValues() {
    qt_config->beginGroup("Controls");
    for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
        Settings::values.input_mappings[Settings::NativeInput::All[i]] =
            qt_config->value(QString::fromStdString(Settings::NativeInput::Mapping[i]), defaults[i]).toInt();
    }
    qt_config->endGroup();

    qt_config->beginGroup("Core");
    Settings::values.frame_skip = qt_config->value("frame_skip", 0).toInt();
    qt_config->endGroup();

    qt_config->beginGroup("Renderer");
    Settings::values.use_hw_renderer = qt_config->value("use_hw_renderer", false).toBool();

    Settings::values.bg_red   = qt_config->value("bg_red",   1.0).toFloat();
    Settings::values.bg_green = qt_config->value("bg_green", 1.0).toFloat();
    Settings::values.bg_blue  = qt_config->value("bg_blue",  1.0).toFloat();
    qt_config->endGroup();

    qt_config->beginGroup("Data Storage");
    Settings::values.use_virtual_sd = qt_config->value("use_virtual_sd", true).toBool();
    qt_config->endGroup();

    qt_config->beginGroup("System Region");
    Settings::values.region_value = qt_config->value("region_value", 1).toInt();
    qt_config->endGroup();

    qt_config->beginGroup("Miscellaneous");
    Settings::values.log_filter = qt_config->value("log_filter", "*:Info").toString().toStdString();
    qt_config->endGroup();
}

void Config::SaveValues() {
    qt_config->beginGroup("Controls");
    for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
        qt_config->setValue(QString::fromStdString(Settings::NativeInput::Mapping[i]),
            Settings::values.input_mappings[Settings::NativeInput::All[i]]);
    }
    qt_config->endGroup();

    qt_config->beginGroup("Core");
    qt_config->setValue("frame_skip", Settings::values.frame_skip);
    qt_config->endGroup();

    qt_config->beginGroup("Renderer");
    qt_config->setValue("use_hw_renderer", Settings::values.use_hw_renderer);

    // Cast to double because Qt's written float values are not human-readable
    qt_config->setValue("bg_red",   (double)Settings::values.bg_red);
    qt_config->setValue("bg_green", (double)Settings::values.bg_green);
    qt_config->setValue("bg_blue",  (double)Settings::values.bg_blue);
    qt_config->endGroup();

    qt_config->beginGroup("Data Storage");
    qt_config->setValue("use_virtual_sd", Settings::values.use_virtual_sd);
    qt_config->endGroup();

    qt_config->beginGroup("System Region");
    qt_config->setValue("region_value", Settings::values.region_value);
    qt_config->endGroup();

    qt_config->beginGroup("Miscellaneous");
    qt_config->setValue("log_filter", QString::fromStdString(Settings::values.log_filter));
    qt_config->endGroup();
}

void Config::Reload() {
    ReadValues();
}

void Config::Save() {
    SaveValues();
}

Config::~Config() {
    Save();

    delete qt_config;
}
