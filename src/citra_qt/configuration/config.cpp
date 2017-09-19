// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QSettings>
#include "citra_qt/configuration/config.h"
#include "citra_qt/ui_settings.h"
#include "common/file_util.h"
#include "input_common/main.h"

Config::Config() {
    // TODO: Don't hardcode the path; let the frontend decide where to put the config files.
    qt_config_loc = FileUtil::GetUserPath(D_CONFIG_IDX) + "qt-config.ini";
    FileUtil::CreateFullPath(qt_config_loc);
    qt_config = new QSettings(QString::fromStdString(qt_config_loc), QSettings::IniFormat);

    Reload();
}

const std::array<int, Settings::NativeButton::NumButtons> Config::default_buttons = {
    Qt::Key_A, Qt::Key_S, Qt::Key_Z, Qt::Key_X, Qt::Key_T, Qt::Key_G, Qt::Key_F, Qt::Key_H,
    Qt::Key_Q, Qt::Key_W, Qt::Key_M, Qt::Key_N, Qt::Key_1, Qt::Key_2, Qt::Key_B,
};

const std::array<std::array<int, 5>, Settings::NativeAnalog::NumAnalogs> Config::default_analogs{{
    {
        Qt::Key_Up, Qt::Key_Down, Qt::Key_Left, Qt::Key_Right, Qt::Key_D,
    },
    {
        Qt::Key_I, Qt::Key_K, Qt::Key_J, Qt::Key_L, Qt::Key_D,
    },
}};

void Config::ReadValues() {
    qt_config->beginGroup("Controls");
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        Settings::values.buttons[i] =
            qt_config
                ->value(Settings::NativeButton::mapping[i], QString::fromStdString(default_param))
                .toString()
                .toStdString();
        if (Settings::values.buttons[i].empty())
            Settings::values.buttons[i] = default_param;
    }

    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_analogs[i][4], 0.5f);
        Settings::values.analogs[i] =
            qt_config
                ->value(Settings::NativeAnalog::mapping[i], QString::fromStdString(default_param))
                .toString()
                .toStdString();
        if (Settings::values.analogs[i].empty())
            Settings::values.analogs[i] = default_param;
    }

    Settings::values.motion_device =
        qt_config->value("motion_device", "engine:motion_emu,update_period:100,sensitivity:0.01")
            .toString()
            .toStdString();
    Settings::values.touch_device =
        qt_config->value("touch_device", "engine:emu_window").toString().toStdString();

    qt_config->endGroup();

    qt_config->beginGroup("Core");
    Settings::values.use_cpu_jit = qt_config->value("use_cpu_jit", true).toBool();
    qt_config->endGroup();

    qt_config->beginGroup("Renderer");
    Settings::values.use_hw_renderer = qt_config->value("use_hw_renderer", true).toBool();
    Settings::values.use_shader_jit = qt_config->value("use_shader_jit", true).toBool();
    Settings::values.resolution_factor = qt_config->value("resolution_factor", 1.0).toFloat();
    Settings::values.use_vsync = qt_config->value("use_vsync", false).toBool();
    Settings::values.toggle_framelimit = qt_config->value("toggle_framelimit", true).toBool();

    Settings::values.bg_red = qt_config->value("bg_red", 0.0).toFloat();
    Settings::values.bg_green = qt_config->value("bg_green", 0.0).toFloat();
    Settings::values.bg_blue = qt_config->value("bg_blue", 0.0).toFloat();
    qt_config->endGroup();

    qt_config->beginGroup("Layout");
    Settings::values.layout_option =
        static_cast<Settings::LayoutOption>(qt_config->value("layout_option").toInt());
    Settings::values.swap_screen = qt_config->value("swap_screen", false).toBool();
    Settings::values.custom_layout = qt_config->value("custom_layout", false).toBool();
    Settings::values.custom_top_left = qt_config->value("custom_top_left", 0).toInt();
    Settings::values.custom_top_top = qt_config->value("custom_top_top", 0).toInt();
    Settings::values.custom_top_right = qt_config->value("custom_top_right", 400).toInt();
    Settings::values.custom_top_bottom = qt_config->value("custom_top_bottom", 240).toInt();
    Settings::values.custom_bottom_left = qt_config->value("custom_bottom_left", 40).toInt();
    Settings::values.custom_bottom_top = qt_config->value("custom_bottom_top", 240).toInt();
    Settings::values.custom_bottom_right = qt_config->value("custom_bottom_right", 360).toInt();
    Settings::values.custom_bottom_bottom = qt_config->value("custom_bottom_bottom", 480).toInt();
    qt_config->endGroup();

    qt_config->beginGroup("Audio");
    Settings::values.sink_id = qt_config->value("output_engine", "auto").toString().toStdString();
    Settings::values.enable_audio_stretching =
        qt_config->value("enable_audio_stretching", true).toBool();
    Settings::values.audio_device_id =
        qt_config->value("output_device", "auto").toString().toStdString();
    qt_config->endGroup();

    using namespace Service::CAM;
    qt_config->beginGroup("Camera");
    Settings::values.camera_name[OuterRightCamera] =
        qt_config->value("camera_outer_right_name", "blank").toString().toStdString();
    Settings::values.camera_config[OuterRightCamera] =
        qt_config->value("camera_outer_right_config", "").toString().toStdString();
    Settings::values.camera_name[InnerCamera] =
        qt_config->value("camera_inner_name", "blank").toString().toStdString();
    Settings::values.camera_config[InnerCamera] =
        qt_config->value("camera_inner_config", "").toString().toStdString();
    Settings::values.camera_name[OuterLeftCamera] =
        qt_config->value("camera_outer_left_name", "blank").toString().toStdString();
    Settings::values.camera_config[OuterLeftCamera] =
        qt_config->value("camera_outer_left_config", "").toString().toStdString();
    qt_config->endGroup();

    qt_config->beginGroup("Data Storage");
    Settings::values.use_virtual_sd = qt_config->value("use_virtual_sd", true).toBool();
    qt_config->endGroup();

    qt_config->beginGroup("System");
    Settings::values.is_new_3ds = qt_config->value("is_new_3ds", false).toBool();
    Settings::values.region_value =
        qt_config->value("region_value", Settings::REGION_VALUE_AUTO_SELECT).toInt();
    qt_config->endGroup();

    qt_config->beginGroup("Miscellaneous");
    Settings::values.log_filter = qt_config->value("log_filter", "*:Info").toString().toStdString();
    qt_config->endGroup();

    qt_config->beginGroup("Debugging");
    Settings::values.use_gdbstub = qt_config->value("use_gdbstub", false).toBool();
    Settings::values.gdbstub_port = qt_config->value("gdbstub_port", 24689).toInt();
    qt_config->endGroup();

    qt_config->beginGroup("WebService");
    Settings::values.enable_telemetry = qt_config->value("enable_telemetry", true).toBool();
    Settings::values.telemetry_endpoint_url =
        qt_config->value("telemetry_endpoint_url", "https://services.citra-emu.org/api/telemetry")
            .toString()
            .toStdString();
    Settings::values.verify_endpoint_url =
        qt_config->value("verify_endpoint_url", "https://services.citra-emu.org/api/profile")
            .toString()
            .toStdString();
    Settings::values.citra_username = qt_config->value("citra_username").toString().toStdString();
    Settings::values.citra_token = qt_config->value("citra_token").toString().toStdString();
    qt_config->endGroup();

    qt_config->beginGroup("UI");
    UISettings::values.theme = qt_config->value("theme", UISettings::themes[0].second).toString();

    qt_config->beginGroup("UILayout");
    UISettings::values.geometry = qt_config->value("geometry").toByteArray();
    UISettings::values.state = qt_config->value("state").toByteArray();
    UISettings::values.renderwindow_geometry =
        qt_config->value("geometryRenderWindow").toByteArray();
    UISettings::values.gamelist_header_state =
        qt_config->value("gameListHeaderState").toByteArray();
    UISettings::values.microprofile_geometry =
        qt_config->value("microProfileDialogGeometry").toByteArray();
    UISettings::values.microprofile_visible =
        qt_config->value("microProfileDialogVisible", false).toBool();
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
            UISettings::values.shortcuts.emplace_back(UISettings::Shortcut(
                group + "/" + hotkey,
                UISettings::ContextualShortcut(qt_config->value("KeySeq").toString(),
                                               qt_config->value("Context").toInt())));
            qt_config->endGroup();
        }

        qt_config->endGroup();
    }
    qt_config->endGroup();

    UISettings::values.single_window_mode = qt_config->value("singleWindowMode", true).toBool();
    UISettings::values.display_titlebar = qt_config->value("displayTitleBars", true).toBool();
    UISettings::values.show_filter_bar = qt_config->value("showFilterBar", true).toBool();
    UISettings::values.show_status_bar = qt_config->value("showStatusBar", true).toBool();
    UISettings::values.confirm_before_closing = qt_config->value("confirmClose", true).toBool();
    UISettings::values.first_start = qt_config->value("firstStart", true).toBool();
    UISettings::values.callout_flags = qt_config->value("calloutFlags", 0).toUInt();

    qt_config->endGroup();
}

void Config::SaveValues() {
    qt_config->beginGroup("Controls");
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        qt_config->setValue(QString::fromStdString(Settings::NativeButton::mapping[i]),
                            QString::fromStdString(Settings::values.buttons[i]));
    }
    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        qt_config->setValue(QString::fromStdString(Settings::NativeAnalog::mapping[i]),
                            QString::fromStdString(Settings::values.analogs[i]));
    }
    qt_config->setValue("motion_device", QString::fromStdString(Settings::values.motion_device));
    qt_config->setValue("touch_device", QString::fromStdString(Settings::values.touch_device));
    qt_config->endGroup();

    qt_config->beginGroup("Core");
    qt_config->setValue("use_cpu_jit", Settings::values.use_cpu_jit);
    qt_config->endGroup();

    qt_config->beginGroup("Renderer");
    qt_config->setValue("use_hw_renderer", Settings::values.use_hw_renderer);
    qt_config->setValue("use_shader_jit", Settings::values.use_shader_jit);
    qt_config->setValue("resolution_factor", (double)Settings::values.resolution_factor);
    qt_config->setValue("use_vsync", Settings::values.use_vsync);
    qt_config->setValue("toggle_framelimit", Settings::values.toggle_framelimit);

    // Cast to double because Qt's written float values are not human-readable
    qt_config->setValue("bg_red", (double)Settings::values.bg_red);
    qt_config->setValue("bg_green", (double)Settings::values.bg_green);
    qt_config->setValue("bg_blue", (double)Settings::values.bg_blue);
    qt_config->endGroup();

    qt_config->beginGroup("Layout");
    qt_config->setValue("layout_option", static_cast<int>(Settings::values.layout_option));
    qt_config->setValue("swap_screen", Settings::values.swap_screen);
    qt_config->setValue("custom_layout", Settings::values.custom_layout);
    qt_config->setValue("custom_top_left", Settings::values.custom_top_left);
    qt_config->setValue("custom_top_top", Settings::values.custom_top_top);
    qt_config->setValue("custom_top_right", Settings::values.custom_top_right);
    qt_config->setValue("custom_top_bottom", Settings::values.custom_top_bottom);
    qt_config->setValue("custom_bottom_left", Settings::values.custom_bottom_left);
    qt_config->setValue("custom_bottom_top", Settings::values.custom_bottom_top);
    qt_config->setValue("custom_bottom_right", Settings::values.custom_bottom_right);
    qt_config->setValue("custom_bottom_bottom", Settings::values.custom_bottom_bottom);
    qt_config->endGroup();

    qt_config->beginGroup("Audio");
    qt_config->setValue("output_engine", QString::fromStdString(Settings::values.sink_id));
    qt_config->setValue("enable_audio_stretching", Settings::values.enable_audio_stretching);
    qt_config->setValue("output_device", QString::fromStdString(Settings::values.audio_device_id));
    qt_config->endGroup();

    using namespace Service::CAM;
    qt_config->beginGroup("Camera");
    qt_config->setValue("camera_outer_right_name",
                        QString::fromStdString(Settings::values.camera_name[OuterRightCamera]));
    qt_config->setValue("camera_outer_right_config",
                        QString::fromStdString(Settings::values.camera_config[OuterRightCamera]));
    qt_config->setValue("camera_inner_name",
                        QString::fromStdString(Settings::values.camera_name[InnerCamera]));
    qt_config->setValue("camera_inner_config",
                        QString::fromStdString(Settings::values.camera_config[InnerCamera]));
    qt_config->setValue("camera_outer_left_name",
                        QString::fromStdString(Settings::values.camera_name[OuterLeftCamera]));
    qt_config->setValue("camera_outer_left_config",
                        QString::fromStdString(Settings::values.camera_config[OuterLeftCamera]));
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

    qt_config->beginGroup("WebService");
    qt_config->setValue("enable_telemetry", Settings::values.enable_telemetry);
    qt_config->setValue("telemetry_endpoint_url",
                        QString::fromStdString(Settings::values.telemetry_endpoint_url));
    qt_config->setValue("verify_endpoint_url",
                        QString::fromStdString(Settings::values.verify_endpoint_url));
    qt_config->setValue("citra_username", QString::fromStdString(Settings::values.citra_username));
    qt_config->setValue("citra_token", QString::fromStdString(Settings::values.citra_token));
    qt_config->endGroup();

    qt_config->beginGroup("UI");
    qt_config->setValue("theme", UISettings::values.theme);

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
    qt_config->setValue("showFilterBar", UISettings::values.show_filter_bar);
    qt_config->setValue("showStatusBar", UISettings::values.show_status_bar);
    qt_config->setValue("confirmClose", UISettings::values.confirm_before_closing);
    qt_config->setValue("firstStart", UISettings::values.first_start);
    qt_config->setValue("calloutFlags", UISettings::values.callout_flags);

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
