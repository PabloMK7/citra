// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QString>
#include <QStringList>

#include "core/settings.h"
#include "core/core.h"
#include "common/file_util.h"

#include "config.h"

Config::Config() {

    // TODO: Don't hardcode the path; let the frontend decide where to put the config files.
    qt_config_loc = FileUtil::GetUserPath(D_CONFIG_IDX) + "qt-config.ini";
    FileUtil::CreateFullPath(qt_config_loc);
    qt_config = new QSettings(QString::fromStdString(qt_config_loc), QSettings::IniFormat);

    Reload();
}

void Config::ReadValues() {
    qt_config->beginGroup("Controls");
    Settings::values.pad_a_key      = qt_config->value("pad_a",      Qt::Key_A).toInt();
    Settings::values.pad_b_key      = qt_config->value("pad_b",      Qt::Key_S).toInt();
    Settings::values.pad_x_key      = qt_config->value("pad_x",      Qt::Key_Z).toInt();
    Settings::values.pad_y_key      = qt_config->value("pad_y",      Qt::Key_X).toInt();
    Settings::values.pad_l_key      = qt_config->value("pad_l",      Qt::Key_Q).toInt();
    Settings::values.pad_r_key      = qt_config->value("pad_r",      Qt::Key_W).toInt();
    Settings::values.pad_zl_key     = qt_config->value("pad_zl",     Qt::Key_1).toInt();
    Settings::values.pad_zr_key     = qt_config->value("pad_zr",     Qt::Key_2).toInt();
    Settings::values.pad_start_key  = qt_config->value("pad_start",  Qt::Key_M).toInt();
    Settings::values.pad_select_key = qt_config->value("pad_select", Qt::Key_N).toInt();
    Settings::values.pad_home_key   = qt_config->value("pad_home",   Qt::Key_B).toInt();
    Settings::values.pad_dup_key    = qt_config->value("pad_dup",    Qt::Key_T).toInt();
    Settings::values.pad_ddown_key  = qt_config->value("pad_ddown",  Qt::Key_G).toInt();
    Settings::values.pad_dleft_key  = qt_config->value("pad_dleft",  Qt::Key_F).toInt();
    Settings::values.pad_dright_key = qt_config->value("pad_dright", Qt::Key_H).toInt();
    Settings::values.pad_sup_key    = qt_config->value("pad_sup",    Qt::Key_Up).toInt();
    Settings::values.pad_sdown_key  = qt_config->value("pad_sdown",  Qt::Key_Down).toInt();
    Settings::values.pad_sleft_key  = qt_config->value("pad_sleft",  Qt::Key_Left).toInt();
    Settings::values.pad_sright_key = qt_config->value("pad_sright", Qt::Key_Right).toInt();
    Settings::values.pad_cup_key    = qt_config->value("pad_cup",    Qt::Key_I).toInt();
    Settings::values.pad_cdown_key  = qt_config->value("pad_cdown",  Qt::Key_K).toInt();
    Settings::values.pad_cleft_key  = qt_config->value("pad_cleft",  Qt::Key_J).toInt();
    Settings::values.pad_cright_key = qt_config->value("pad_cright", Qt::Key_L).toInt();
    qt_config->endGroup();

    qt_config->beginGroup("Core");
    Settings::values.gpu_refresh_rate = qt_config->value("gpu_refresh_rate", 30).toInt();
    Settings::values.frame_skip = qt_config->value("frame_skip", 0).toInt();
    qt_config->endGroup();

    qt_config->beginGroup("Renderer");
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
    qt_config->setValue("pad_a",      Settings::values.pad_a_key);
    qt_config->setValue("pad_b",      Settings::values.pad_b_key);
    qt_config->setValue("pad_x",      Settings::values.pad_x_key);
    qt_config->setValue("pad_y",      Settings::values.pad_y_key);
    qt_config->setValue("pad_l",      Settings::values.pad_l_key);
    qt_config->setValue("pad_r",      Settings::values.pad_r_key);
    qt_config->setValue("pad_zl",     Settings::values.pad_zl_key);
    qt_config->setValue("pad_zr",     Settings::values.pad_zr_key);
    qt_config->setValue("pad_start",  Settings::values.pad_start_key);
    qt_config->setValue("pad_select", Settings::values.pad_select_key);
    qt_config->setValue("pad_home",   Settings::values.pad_home_key);
    qt_config->setValue("pad_dup",    Settings::values.pad_dup_key);
    qt_config->setValue("pad_ddown",  Settings::values.pad_ddown_key);
    qt_config->setValue("pad_dleft",  Settings::values.pad_dleft_key);
    qt_config->setValue("pad_dright", Settings::values.pad_dright_key);
    qt_config->setValue("pad_sup",    Settings::values.pad_sup_key);
    qt_config->setValue("pad_sdown",  Settings::values.pad_sdown_key);
    qt_config->setValue("pad_sleft",  Settings::values.pad_sleft_key);
    qt_config->setValue("pad_sright", Settings::values.pad_sright_key);
    qt_config->setValue("pad_cup",    Settings::values.pad_cup_key);
    qt_config->setValue("pad_cdown",  Settings::values.pad_cdown_key);
    qt_config->setValue("pad_cleft",  Settings::values.pad_cleft_key);
    qt_config->setValue("pad_cright", Settings::values.pad_cright_key);
    qt_config->endGroup();

    qt_config->beginGroup("Core");
    qt_config->setValue("gpu_refresh_rate", Settings::values.gpu_refresh_rate);
    qt_config->setValue("frame_skip", Settings::values.frame_skip);
    qt_config->endGroup();

    qt_config->beginGroup("Renderer");
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
