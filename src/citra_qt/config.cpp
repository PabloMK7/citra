// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QSettings>

#include "citra_qt/config.h"
#include "citra_qt/ui_settings.h"

#include "common/file_util.h"

Config::Config() {
    // TODO: Don't hardcode the path; let the frontend decide where to put the config files.
    qt_config_loc = FileUtil::GetUserPath(D_CONFIG_IDX) + "qt-config.ini";
    FileUtil::CreateFullPath(qt_config_loc);
    qt_config = new QSettings(QString::fromStdString(qt_config_loc), QSettings::IniFormat);

    Reload();
}

const std::array<QVariant, Settings::NativeInput::NUM_INPUTS> Config::defaults = {
    // directly mapped keys
    Qt::Key_A, Qt::Key_S, Qt::Key_Z, Qt::Key_X,
    Qt::Key_Q, Qt::Key_W, Qt::Key_1, Qt::Key_2,
    Qt::Key_M, Qt::Key_N, Qt::Key_B,
    Qt::Key_T, Qt::Key_G, Qt::Key_F, Qt::Key_H,
    Qt::Key_I, Qt::Key_K, Qt::Key_J, Qt::Key_L,

    // indirectly mapped keys
    Qt::Key_Up, Qt::Key_Down, Qt::Key_Left, Qt::Key_Right,
    Qt::Key_D,
};

void Config::ReadValues() {
    qt_config->beginGroup("Controls");
    for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
        Settings::values.input_mappings[Settings::NativeInput::All[i]] =
            qt_config->value(QString::fromStdString(Settings::NativeInput::Mapping[i]), defaults[i]).toInt();
    }
    Settings::values.pad_circle_modifier_scale = qt_config->value("pad_circle_modifier_scale", 0.5).toFloat();
    qt_config->endGroup();

    qt_config->beginGroup("Core");
    Settings::values.frame_skip = qt_config->value("frame_skip", 0).toInt();
    qt_config->endGroup();

    qt_config->beginGroup("Renderer");
    Settings::values.use_hw_renderer = qt_config->value("use_hw_renderer", true).toBool();
    Settings::values.use_shader_jit = qt_config->value("use_shader_jit", true).toBool();
    Settings::values.use_scaled_resolution = qt_config->value("use_scaled_resolution", false).toBool();
    Settings::values.use_vsync = qt_config->value("use_vsync", false).toBool();

    Settings::values.bg_red   = qt_config->value("bg_red",   1.0).toFloat();
    Settings::values.bg_green = qt_config->value("bg_green", 1.0).toFloat();
    Settings::values.bg_blue  = qt_config->value("bg_blue",  1.0).toFloat();
    qt_config->endGroup();

    qt_config->beginGroup("Audio");
    Settings::values.sink_id = qt_config->value("output_engine", "auto").toString().toStdString();
    Settings::values.enable_audio_stretching = qt_config->value("enable_audio_stretching", true).toBool();
    qt_config->endGroup();

    qt_config->beginGroup("Data Storage");
    Settings::values.use_virtual_sd = qt_config->value("use_virtual_sd", true).toBool();
    qt_config->endGroup();

    qt_config->beginGroup("System");
    Settings::values.is_new_3ds = qt_config->value("is_new_3ds", false).toBool();
    Settings::values.region_value = qt_config->value("region_value", 1).toInt();
    qt_config->endGroup();

    qt_config->beginGroup("Miscellaneous");
    Settings::values.log_filter = qt_config->value("log_filter", "*:Info").toString().toStdString();
    qt_config->endGroup();

    qt_config->beginGroup("Debugging");
    Settings::values.use_gdbstub = qt_config->value("use_gdbstub", false).toBool();
    Settings::values.gdbstub_port = qt_config->value("gdbstub_port", 24689).toInt();
    qt_config->endGroup();

    qt_config->beginGroup("UI");

    qt_config->beginGroup("UILayout");
    UISettings::values.geometry = qt_config->value("geometry").toByteArray();
    UISettings::values.state = qt_config->value("state").toByteArray();
    UISettings::values.renderwindow_geometry = qt_config->value("geometryRenderWindow").toByteArray();
    UISettings::values.gamelist_header_state = qt_config->value("gameListHeaderState").toByteArray();
    UISettings::values.microprofile_geometry = qt_config->value("microProfileDialogGeometry").toByteArray();
    UISettings::values.microprofile_visible = qt_config->value("microProfileDialogVisible", false).toBool();
    qt_config->endGroup();

    qt_config->beginGroup("Paths");
    UISettings::values.roms_path = qt_config->value("romsPath").toString();
    UISettings::values.symbols_path = qt_config->value("symbolsPath").toString();
    UISettings::values.gamedir = qt_config->value("gameListRootDir", ".").toString();
    UISettings::values.gamedir_deepscan = qt_config->value("gameListDeepScan", false).toBool();
    UISettings::values.recent_files = qt_config->value("recentFiles").toStringList();
    qt_config->endGroup();

    qt_config->beginGroup("Shortcuts");
    QStringList groups = qt_config->childGroups();
    for (auto group : groups) {
        qt_config->beginGroup(group);

        QStringList hotkeys = qt_config->childGroups();
        for (auto hotkey : hotkeys) {
            qt_config->beginGroup(hotkey);
            UISettings::values.shortcuts.emplace_back(
                        UISettings::Shortcut(group + "/" + hotkey,
                                             UISettings::ContextualShortcut(qt_config->value("KeySeq").toString(),
                                                                            qt_config->value("Context").toInt())));
            qt_config->endGroup();
        }

        qt_config->endGroup();
    }
    qt_config->endGroup();

    UISettings::values.single_window_mode = qt_config->value("singleWindowMode", true).toBool();
    UISettings::values.display_titlebar = qt_config->value("displayTitleBars", true).toBool();
    UISettings::values.confirm_before_closing = qt_config->value("confirmClose",true).toBool();
    UISettings::values.first_start = qt_config->value("firstStart", true).toBool();

    qt_config->endGroup();
}

void Config::SaveValues() {
    qt_config->beginGroup("Controls");
    for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
        qt_config->setValue(QString::fromStdString(Settings::NativeInput::Mapping[i]),
            Settings::values.input_mappings[Settings::NativeInput::All[i]]);
    }
    qt_config->setValue("pad_circle_modifier_scale", (double)Settings::values.pad_circle_modifier_scale);
    qt_config->endGroup();

    qt_config->beginGroup("Core");
    qt_config->setValue("frame_skip", Settings::values.frame_skip);
    qt_config->endGroup();

    qt_config->beginGroup("Renderer");
    qt_config->setValue("use_hw_renderer", Settings::values.use_hw_renderer);
    qt_config->setValue("use_shader_jit", Settings::values.use_shader_jit);
    qt_config->setValue("use_scaled_resolution", Settings::values.use_scaled_resolution);
    qt_config->setValue("use_vsync", Settings::values.use_vsync);

    // Cast to double because Qt's written float values are not human-readable
    qt_config->setValue("bg_red",   (double)Settings::values.bg_red);
    qt_config->setValue("bg_green", (double)Settings::values.bg_green);
    qt_config->setValue("bg_blue",  (double)Settings::values.bg_blue);
    qt_config->endGroup();

    qt_config->beginGroup("Audio");
    qt_config->setValue("output_engine", QString::fromStdString(Settings::values.sink_id));
    qt_config->setValue("enable_audio_stretching", Settings::values.enable_audio_stretching);
    qt_config->endGroup();

    qt_config->beginGroup("Data Storage");
    qt_config->setValue("use_virtual_sd", Settings::values.use_virtual_sd);
    qt_config->endGroup();

    qt_config->beginGroup("System");
    qt_config->setValue("is_new_3ds", Settings::values.is_new_3ds);
    qt_config->setValue("region_value", Settings::values.region_value);
    qt_config->endGroup();

    qt_config->beginGroup("Miscellaneous");
    qt_config->setValue("log_filter", QString::fromStdString(Settings::values.log_filter));
    qt_config->endGroup();

    qt_config->beginGroup("Debugging");
    qt_config->setValue("use_gdbstub", Settings::values.use_gdbstub);
    qt_config->setValue("gdbstub_port", Settings::values.gdbstub_port);
    qt_config->endGroup();

    qt_config->beginGroup("UI");

    qt_config->beginGroup("UILayout");
    qt_config->setValue("geometry", UISettings::values.geometry);
    qt_config->setValue("state", UISettings::values.state);
    qt_config->setValue("geometryRenderWindow", UISettings::values.renderwindow_geometry);
    qt_config->setValue("gameListHeaderState", UISettings::values.gamelist_header_state);
    qt_config->setValue("microProfileDialogGeometry", UISettings::values.microprofile_geometry);
    qt_config->setValue("microProfileDialogVisible", UISettings::values.microprofile_visible);
    qt_config->endGroup();

    qt_config->beginGroup("Paths");
    qt_config->setValue("romsPath", UISettings::values.roms_path);
    qt_config->setValue("symbolsPath", UISettings::values.symbols_path);
    qt_config->setValue("gameListRootDir", UISettings::values.gamedir);
    qt_config->setValue("gameListDeepScan", UISettings::values.gamedir_deepscan);
    qt_config->setValue("recentFiles", UISettings::values.recent_files);
    qt_config->endGroup();

    qt_config->beginGroup("Shortcuts");
    for (auto shortcut : UISettings::values.shortcuts) {
        qt_config->setValue(shortcut.first + "/KeySeq", shortcut.second.first);
        qt_config->setValue(shortcut.first + "/Context", shortcut.second.second);
    }
    qt_config->endGroup();

    qt_config->setValue("singleWindowMode", UISettings::values.single_window_mode);
    qt_config->setValue("displayTitleBars", UISettings::values.display_titlebar);
    qt_config->setValue("confirmClose", UISettings::values.confirm_before_closing);
    qt_config->setValue("firstStart", UISettings::values.first_start);

    qt_config->endGroup();
}

void Config::Reload() {
    ReadValues();
    Settings::Apply();
}

void Config::Save() {
    SaveValues();
}

Config::~Config() {
    Save();

    delete qt_config;
}
