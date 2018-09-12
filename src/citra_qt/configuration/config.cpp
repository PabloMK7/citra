// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <unordered_map>
#include <QSettings>
#include "citra_qt/configuration/config.h"
#include "citra_qt/ui_settings.h"
#include "common/file_util.h"
#include "core/hle/service/service.h"
#include "input_common/main.h"
#include "input_common/udp/client.h"
#include "network/network.h"

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
        Qt::Key_Up,
        Qt::Key_Down,
        Qt::Key_Left,
        Qt::Key_Right,
        Qt::Key_D,
    },
    {
        Qt::Key_I,
        Qt::Key_K,
        Qt::Key_J,
        Qt::Key_L,
        Qt::Key_D,
    },
}};

void Config::ReadValues() {
    qt_config->beginGroup("Controls");
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        Settings::values.buttons[i] =
            ReadSetting(Settings::NativeButton::mapping[i], QString::fromStdString(default_param))
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
            ReadSetting(Settings::NativeAnalog::mapping[i], QString::fromStdString(default_param))
                .toString()
                .toStdString();
        if (Settings::values.analogs[i].empty())
            Settings::values.analogs[i] = default_param;
    }

    Settings::values.motion_device =
        ReadSetting("motion_device",
                    "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0")
            .toString()
            .toStdString();
    Settings::values.touch_device =
        ReadSetting("touch_device", "engine:emu_window").toString().toStdString();

    Settings::values.udp_input_address =
        ReadSetting("udp_input_address", InputCommon::CemuhookUDP::DEFAULT_ADDR)
            .toString()
            .toStdString();
    Settings::values.udp_input_port = static_cast<u16>(
        ReadSetting("udp_input_port", InputCommon::CemuhookUDP::DEFAULT_PORT).toInt());
    Settings::values.udp_pad_index = static_cast<u8>(ReadSetting("udp_pad_index", 0).toUInt());

    qt_config->endGroup();

    qt_config->beginGroup("Core");
    Settings::values.use_cpu_jit = ReadSetting("use_cpu_jit", true).toBool();
    qt_config->endGroup();

    qt_config->beginGroup("Renderer");
    Settings::values.use_hw_renderer = ReadSetting("use_hw_renderer", true).toBool();
#ifdef __APPLE__
    // Hardware shader is broken on macos thanks to poor drivers.
    // We still want to provide this option for test/development purposes, but disable it by
    // default.
    Settings::values.use_hw_shader = ReadSetting("use_hw_shader", false).toBool();
#else
    Settings::values.use_hw_shader = ReadSetting("use_hw_shader", true).toBool();
#endif
    Settings::values.shaders_accurate_gs = ReadSetting("shaders_accurate_gs", true).toBool();
    Settings::values.shaders_accurate_mul = ReadSetting("shaders_accurate_mul", false).toBool();
    Settings::values.use_shader_jit = ReadSetting("use_shader_jit", true).toBool();
    Settings::values.resolution_factor =
        static_cast<u16>(ReadSetting("resolution_factor", 1).toInt());
    Settings::values.use_vsync = ReadSetting("use_vsync", false).toBool();
    Settings::values.use_frame_limit = ReadSetting("use_frame_limit", true).toBool();
    Settings::values.frame_limit = ReadSetting("frame_limit", 100).toInt();

    Settings::values.bg_red = ReadSetting("bg_red", 0.0).toFloat();
    Settings::values.bg_green = ReadSetting("bg_green", 0.0).toFloat();
    Settings::values.bg_blue = ReadSetting("bg_blue", 0.0).toFloat();
    qt_config->endGroup();

    qt_config->beginGroup("Layout");
    Settings::values.toggle_3d = ReadSetting("toggle_3d", false).toBool();
    Settings::values.factor_3d = ReadSetting("factor_3d", 0).toInt();
    Settings::values.layout_option =
        static_cast<Settings::LayoutOption>(ReadSetting("layout_option").toInt());
    Settings::values.swap_screen = ReadSetting("swap_screen", false).toBool();
    Settings::values.custom_layout = ReadSetting("custom_layout", false).toBool();
    Settings::values.custom_top_left = ReadSetting("custom_top_left", 0).toInt();
    Settings::values.custom_top_top = ReadSetting("custom_top_top", 0).toInt();
    Settings::values.custom_top_right = ReadSetting("custom_top_right", 400).toInt();
    Settings::values.custom_top_bottom = ReadSetting("custom_top_bottom", 240).toInt();
    Settings::values.custom_bottom_left = ReadSetting("custom_bottom_left", 40).toInt();
    Settings::values.custom_bottom_top = ReadSetting("custom_bottom_top", 240).toInt();
    Settings::values.custom_bottom_right = ReadSetting("custom_bottom_right", 360).toInt();
    Settings::values.custom_bottom_bottom = ReadSetting("custom_bottom_bottom", 480).toInt();
    qt_config->endGroup();

    qt_config->beginGroup("Audio");
    Settings::values.sink_id = ReadSetting("output_engine", "auto").toString().toStdString();
    Settings::values.enable_audio_stretching =
        ReadSetting("enable_audio_stretching", true).toBool();
    Settings::values.audio_device_id =
        ReadSetting("output_device", "auto").toString().toStdString();
    Settings::values.volume = ReadSetting("volume", 1).toFloat();
    qt_config->endGroup();

    using namespace Service::CAM;
    qt_config->beginGroup("Camera");
    Settings::values.camera_name[OuterRightCamera] =
        ReadSetting("camera_outer_right_name", "blank").toString().toStdString();
    Settings::values.camera_config[OuterRightCamera] =
        ReadSetting("camera_outer_right_config", "").toString().toStdString();
    Settings::values.camera_flip[OuterRightCamera] =
        ReadSetting("camera_outer_right_flip", "0").toInt();
    Settings::values.camera_name[InnerCamera] =
        ReadSetting("camera_inner_name", "blank").toString().toStdString();
    Settings::values.camera_config[InnerCamera] =
        ReadSetting("camera_inner_config", "").toString().toStdString();
    Settings::values.camera_flip[InnerCamera] = ReadSetting("camera_inner_flip", "").toInt();
    Settings::values.camera_name[OuterLeftCamera] =
        ReadSetting("camera_outer_left_name", "blank").toString().toStdString();
    Settings::values.camera_config[OuterLeftCamera] =
        ReadSetting("camera_outer_left_config", "").toString().toStdString();
    Settings::values.camera_flip[OuterLeftCamera] =
        ReadSetting("camera_outer_left_flip", "").toInt();
    qt_config->endGroup();

    qt_config->beginGroup("Data Storage");
    Settings::values.use_virtual_sd = ReadSetting("use_virtual_sd", true).toBool();
    qt_config->endGroup();

    qt_config->beginGroup("System");
    Settings::values.is_new_3ds = ReadSetting("is_new_3ds", false).toBool();
    Settings::values.region_value =
        ReadSetting("region_value", Settings::REGION_VALUE_AUTO_SELECT).toInt();
    Settings::values.init_clock = static_cast<Settings::InitClock>(
        ReadSetting("init_clock", static_cast<u32>(Settings::InitClock::SystemTime)).toInt());
    Settings::values.init_time = ReadSetting("init_time", 946681277ULL).toULongLong();
    qt_config->endGroup();

    qt_config->beginGroup("Miscellaneous");
    Settings::values.log_filter = ReadSetting("log_filter", "*:Info").toString().toStdString();
    qt_config->endGroup();

    qt_config->beginGroup("Debugging");
    Settings::values.use_gdbstub = ReadSetting("use_gdbstub", false).toBool();
    Settings::values.gdbstub_port = ReadSetting("gdbstub_port", 24689).toInt();

    qt_config->beginGroup("LLE");
    for (const auto& service_module : Service::service_module_map) {
        bool use_lle = ReadSetting(QString::fromStdString(service_module.name), false).toBool();
        Settings::values.lle_modules.emplace(service_module.name, use_lle);
    }
    qt_config->endGroup();
    qt_config->endGroup();

    qt_config->beginGroup("WebService");
    Settings::values.enable_telemetry = ReadSetting("enable_telemetry", true).toBool();
    Settings::values.web_api_url =
        ReadSetting("web_api_url", "https://api.citra-emu.org").toString().toStdString();
    Settings::values.citra_username = ReadSetting("citra_username").toString().toStdString();
    Settings::values.citra_token = ReadSetting("citra_token").toString().toStdString();
    qt_config->endGroup();

    qt_config->beginGroup("UI");
    UISettings::values.theme = ReadSetting("theme", UISettings::themes[0].second).toString();
    UISettings::values.enable_discord_presence =
        ReadSetting("enable_discord_presence", true).toBool();

    qt_config->beginGroup("Updater");
    UISettings::values.check_for_update_on_start =
        ReadSetting("check_for_update_on_start", true).toBool();
    UISettings::values.update_on_close = ReadSetting("update_on_close", false).toBool();
    qt_config->endGroup();

    qt_config->beginGroup("UILayout");
    UISettings::values.geometry = ReadSetting("geometry").toByteArray();
    UISettings::values.state = ReadSetting("state").toByteArray();
    UISettings::values.renderwindow_geometry = ReadSetting("geometryRenderWindow").toByteArray();
    UISettings::values.gamelist_header_state = ReadSetting("gameListHeaderState").toByteArray();
    UISettings::values.microprofile_geometry =
        ReadSetting("microProfileDialogGeometry").toByteArray();
    UISettings::values.microprofile_visible =
        ReadSetting("microProfileDialogVisible", false).toBool();
    qt_config->endGroup();

    qt_config->beginGroup("Paths");
    UISettings::values.roms_path = ReadSetting("romsPath").toString();
    UISettings::values.symbols_path = ReadSetting("symbolsPath").toString();
    UISettings::values.movie_record_path = ReadSetting("movieRecordPath").toString();
    UISettings::values.movie_playback_path = ReadSetting("moviePlaybackPath").toString();
    UISettings::values.game_dir_deprecated = ReadSetting("gameListRootDir", ".").toString();
    UISettings::values.game_dir_deprecated_deepscan =
        ReadSetting("gameListDeepScan", false).toBool();
    int size = qt_config->beginReadArray("gamedirs");
    for (int i = 0; i < size; ++i) {
        qt_config->setArrayIndex(i);
        UISettings::GameDir game_dir;
        game_dir.path = ReadSetting("path").toString();
        game_dir.deep_scan = ReadSetting("deep_scan", false).toBool();
        game_dir.expanded = ReadSetting("expanded", true).toBool();
        UISettings::values.game_dirs.append(game_dir);
    }
    qt_config->endArray();
    // create NAND and SD card directories if empty, these are not removable through the UI, also
    // carries over old game list settings if present
    if (UISettings::values.game_dirs.isEmpty()) {
        UISettings::GameDir game_dir;
        game_dir.path = "INSTALLED";
        game_dir.expanded = true;
        UISettings::values.game_dirs.append(game_dir);
        game_dir.path = "SYSTEM";
        UISettings::values.game_dirs.append(game_dir);
        if (UISettings::values.game_dir_deprecated != ".") {
            game_dir.path = UISettings::values.game_dir_deprecated;
            game_dir.deep_scan = UISettings::values.game_dir_deprecated_deepscan;
            UISettings::values.game_dirs.append(game_dir);
        }
    }
    UISettings::values.recent_files = ReadSetting("recentFiles").toStringList();
    UISettings::values.language = ReadSetting("language", "").toString();
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
                UISettings::ContextualShortcut(ReadSetting("KeySeq").toString(),
                                               ReadSetting("Context").toInt())));
            qt_config->endGroup();
        }

        qt_config->endGroup();
    }
    qt_config->endGroup();

    UISettings::values.single_window_mode = ReadSetting("singleWindowMode", true).toBool();
    UISettings::values.fullscreen = ReadSetting("fullscreen", false).toBool();
    UISettings::values.display_titlebar = ReadSetting("displayTitleBars", true).toBool();
    UISettings::values.show_filter_bar = ReadSetting("showFilterBar", true).toBool();
    UISettings::values.show_status_bar = ReadSetting("showStatusBar", true).toBool();
    UISettings::values.confirm_before_closing = ReadSetting("confirmClose", true).toBool();
    UISettings::values.first_start = ReadSetting("firstStart", true).toBool();
    UISettings::values.callout_flags = ReadSetting("calloutFlags", 0).toUInt();
    UISettings::values.show_console = ReadSetting("showConsole", false).toBool();

    qt_config->beginGroup("Multiplayer");
    UISettings::values.nickname = ReadSetting("nickname", "").toString();
    UISettings::values.ip = ReadSetting("ip", "").toString();
    UISettings::values.port = ReadSetting("port", Network::DefaultRoomPort).toString();
    UISettings::values.room_nickname = ReadSetting("room_nickname", "").toString();
    UISettings::values.room_name = ReadSetting("room_name", "").toString();
    UISettings::values.room_port = ReadSetting("room_port", "24872").toString();
    bool ok;
    UISettings::values.host_type = ReadSetting("host_type", 0).toUInt(&ok);
    if (!ok) {
        UISettings::values.host_type = 0;
    }
    UISettings::values.max_player = ReadSetting("max_player", 8).toUInt();
    UISettings::values.game_id = ReadSetting("game_id", 0).toULongLong();
    qt_config->endGroup();

    qt_config->endGroup();
}

void Config::SaveValues() {
    qt_config->beginGroup("Controls");
    for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
        std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
        WriteSetting(QString::fromStdString(Settings::NativeButton::mapping[i]),
                     QString::fromStdString(Settings::values.buttons[i]),
                     QString::fromStdString(default_param));
    }
    for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
        std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
            default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
            default_analogs[i][3], default_analogs[i][4], 0.5f);
        WriteSetting(QString::fromStdString(Settings::NativeAnalog::mapping[i]),
                     QString::fromStdString(Settings::values.analogs[i]),
                     QString::fromStdString(default_param));
    }
    WriteSetting("motion_device", QString::fromStdString(Settings::values.motion_device),
                 "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0");
    WriteSetting("touch_device", QString::fromStdString(Settings::values.touch_device),
                 "engine:emu_window");
    WriteSetting("udp_input_address", QString::fromStdString(Settings::values.udp_input_address),
                 InputCommon::CemuhookUDP::DEFAULT_ADDR);
    WriteSetting("udp_input_port", Settings::values.udp_input_port,
                 InputCommon::CemuhookUDP::DEFAULT_PORT);
    WriteSetting("udp_pad_index", Settings::values.udp_pad_index, 0);
    qt_config->endGroup();

    qt_config->beginGroup("Core");
    WriteSetting("use_cpu_jit", Settings::values.use_cpu_jit, true);
    qt_config->endGroup();

    qt_config->beginGroup("Renderer");
    WriteSetting("use_hw_renderer", Settings::values.use_hw_renderer, true);
    WriteSetting("use_hw_shader", Settings::values.use_hw_shader, true);
    WriteSetting("shaders_accurate_gs", Settings::values.shaders_accurate_gs, true);
    WriteSetting("shaders_accurate_mul", Settings::values.shaders_accurate_mul, false);
    WriteSetting("use_shader_jit", Settings::values.use_shader_jit, true);
    WriteSetting("resolution_factor", Settings::values.resolution_factor, 1);
    WriteSetting("use_vsync", Settings::values.use_vsync, false);
    WriteSetting("use_frame_limit", Settings::values.use_frame_limit, true);
    WriteSetting("frame_limit", Settings::values.frame_limit, 100);

    // Cast to double because Qt's written float values are not human-readable
    WriteSetting("bg_red", (double)Settings::values.bg_red, 0.0);
    WriteSetting("bg_green", (double)Settings::values.bg_green, 0.0);
    WriteSetting("bg_blue", (double)Settings::values.bg_blue, 0.0);
    qt_config->endGroup();

    qt_config->beginGroup("Layout");
    WriteSetting("toggle_3d", Settings::values.toggle_3d, false);
    WriteSetting("factor_3d", Settings::values.factor_3d, 0);
    WriteSetting("layout_option", static_cast<int>(Settings::values.layout_option));
    WriteSetting("swap_screen", Settings::values.swap_screen, false);
    WriteSetting("custom_layout", Settings::values.custom_layout, false);
    WriteSetting("custom_top_left", Settings::values.custom_top_left, 0);
    WriteSetting("custom_top_top", Settings::values.custom_top_top, 0);
    WriteSetting("custom_top_right", Settings::values.custom_top_right, 400);
    WriteSetting("custom_top_bottom", Settings::values.custom_top_bottom, 240);
    WriteSetting("custom_bottom_left", Settings::values.custom_bottom_left, 40);
    WriteSetting("custom_bottom_top", Settings::values.custom_bottom_top, 240);
    WriteSetting("custom_bottom_right", Settings::values.custom_bottom_right, 360);
    WriteSetting("custom_bottom_bottom", Settings::values.custom_bottom_bottom, 480);
    qt_config->endGroup();

    qt_config->beginGroup("Audio");
    WriteSetting("output_engine", QString::fromStdString(Settings::values.sink_id), "auto");
    WriteSetting("enable_audio_stretching", Settings::values.enable_audio_stretching, true);
    WriteSetting("output_device", QString::fromStdString(Settings::values.audio_device_id), "auto");
    WriteSetting("volume", Settings::values.volume, 1.0f);
    qt_config->endGroup();

    using namespace Service::CAM;
    qt_config->beginGroup("Camera");
    WriteSetting("camera_outer_right_name",
                 QString::fromStdString(Settings::values.camera_name[OuterRightCamera]), "blank");
    WriteSetting("camera_outer_right_config",
                 QString::fromStdString(Settings::values.camera_config[OuterRightCamera]), "");
    WriteSetting("camera_outer_right_flip", Settings::values.camera_flip[OuterRightCamera], 0);
    WriteSetting("camera_inner_name",
                 QString::fromStdString(Settings::values.camera_name[InnerCamera]), "blank");
    WriteSetting("camera_inner_config",
                 QString::fromStdString(Settings::values.camera_config[InnerCamera]), "");
    WriteSetting("camera_inner_flip", Settings::values.camera_flip[InnerCamera], 0);
    WriteSetting("camera_outer_left_name",
                 QString::fromStdString(Settings::values.camera_name[OuterLeftCamera]), "blank");
    WriteSetting("camera_outer_left_config",
                 QString::fromStdString(Settings::values.camera_config[OuterLeftCamera]), "");
    WriteSetting("camera_outer_left_flip", Settings::values.camera_flip[OuterLeftCamera], 0);
    qt_config->endGroup();

    qt_config->beginGroup("Data Storage");
    WriteSetting("use_virtual_sd", Settings::values.use_virtual_sd, true);
    qt_config->endGroup();

    qt_config->beginGroup("System");
    WriteSetting("is_new_3ds", Settings::values.is_new_3ds, false);
    WriteSetting("region_value", Settings::values.region_value, Settings::REGION_VALUE_AUTO_SELECT);
    WriteSetting("init_clock", static_cast<u32>(Settings::values.init_clock),
                 static_cast<u32>(Settings::InitClock::SystemTime));
    WriteSetting("init_time", static_cast<unsigned long long>(Settings::values.init_time),
                 946681277ULL);
    qt_config->endGroup();

    qt_config->beginGroup("Miscellaneous");
    WriteSetting("log_filter", QString::fromStdString(Settings::values.log_filter), "*:Info");
    qt_config->endGroup();

    qt_config->beginGroup("Debugging");
    WriteSetting("use_gdbstub", Settings::values.use_gdbstub, false);
    WriteSetting("gdbstub_port", Settings::values.gdbstub_port, 24689);

    qt_config->beginGroup("LLE");
    for (const auto& service_module : Settings::values.lle_modules) {
        WriteSetting(QString::fromStdString(service_module.first), service_module.second, false);
    }
    qt_config->endGroup();
    qt_config->endGroup();

    qt_config->beginGroup("WebService");
    WriteSetting("enable_telemetry", Settings::values.enable_telemetry, true);
    WriteSetting("web_api_url", QString::fromStdString(Settings::values.web_api_url),
                 "https://api.citra-emu.org");
    WriteSetting("citra_username", QString::fromStdString(Settings::values.citra_username));
    WriteSetting("citra_token", QString::fromStdString(Settings::values.citra_token));
    qt_config->endGroup();

    qt_config->beginGroup("UI");
    WriteSetting("theme", UISettings::values.theme, UISettings::themes[0].second);
    WriteSetting("enable_discord_presence", UISettings::values.enable_discord_presence, true);

    qt_config->beginGroup("Updater");
    WriteSetting("check_for_update_on_start", UISettings::values.check_for_update_on_start, true);
    WriteSetting("update_on_close", UISettings::values.update_on_close, false);
    qt_config->endGroup();

    qt_config->beginGroup("UILayout");
    WriteSetting("geometry", UISettings::values.geometry);
    WriteSetting("state", UISettings::values.state);
    WriteSetting("geometryRenderWindow", UISettings::values.renderwindow_geometry);
    WriteSetting("gameListHeaderState", UISettings::values.gamelist_header_state);
    WriteSetting("microProfileDialogGeometry", UISettings::values.microprofile_geometry);
    WriteSetting("microProfileDialogVisible", UISettings::values.microprofile_visible, false);
    qt_config->endGroup();

    qt_config->beginGroup("Paths");
    WriteSetting("romsPath", UISettings::values.roms_path);
    WriteSetting("symbolsPath", UISettings::values.symbols_path);
    WriteSetting("movieRecordPath", UISettings::values.movie_record_path);
    WriteSetting("moviePlaybackPath", UISettings::values.movie_playback_path);
    qt_config->beginWriteArray("gamedirs");
    for (int i = 0; i < UISettings::values.game_dirs.size(); ++i) {
        qt_config->setArrayIndex(i);
        const auto& game_dir = UISettings::values.game_dirs.at(i);
        WriteSetting("path", game_dir.path);
        WriteSetting("deep_scan", game_dir.deep_scan, false);
        WriteSetting("expanded", game_dir.expanded, true);
    }
    qt_config->endArray();
    WriteSetting("recentFiles", UISettings::values.recent_files);
    WriteSetting("language", UISettings::values.language, "");
    qt_config->endGroup();

    qt_config->beginGroup("Shortcuts");
    for (auto shortcut : UISettings::values.shortcuts) {
        WriteSetting(shortcut.first + "/KeySeq", shortcut.second.first);
        WriteSetting(shortcut.first + "/Context", shortcut.second.second);
    }
    qt_config->endGroup();

    WriteSetting("singleWindowMode", UISettings::values.single_window_mode, true);
    WriteSetting("fullscreen", UISettings::values.fullscreen, false);
    WriteSetting("displayTitleBars", UISettings::values.display_titlebar, true);
    WriteSetting("showFilterBar", UISettings::values.show_filter_bar, true);
    WriteSetting("showStatusBar", UISettings::values.show_status_bar, true);
    WriteSetting("confirmClose", UISettings::values.confirm_before_closing, true);
    WriteSetting("firstStart", UISettings::values.first_start, true);
    WriteSetting("calloutFlags", UISettings::values.callout_flags, 0);
    WriteSetting("showConsole", UISettings::values.show_console, false);

    qt_config->beginGroup("Multiplayer");
    WriteSetting("nickname", UISettings::values.nickname, "");
    WriteSetting("ip", UISettings::values.ip, "");
    WriteSetting("port", UISettings::values.port, Network::DefaultRoomPort);
    WriteSetting("room_nickname", UISettings::values.room_nickname, "");
    WriteSetting("room_name", UISettings::values.room_name, "");
    WriteSetting("room_port", UISettings::values.room_port, "24872");
    WriteSetting("host_type", UISettings::values.host_type, 0);
    WriteSetting("max_player", UISettings::values.max_player, 8);
    WriteSetting("game_id", UISettings::values.game_id, 0);
    qt_config->endGroup();

    qt_config->endGroup();
}

QVariant Config::ReadSetting(const QString& name) {
    return qt_config->value(name);
}

QVariant Config::ReadSetting(const QString& name, const QVariant& default_value) {
    QVariant result;
    if (qt_config->value(name + "/default", false).toBool()) {
        result = default_value;
    } else {
        result = qt_config->value(name, default_value);
    }
    return result;
}

void Config::WriteSetting(const QString& name, const QVariant& value) {
    qt_config->setValue(name, value);
}

void Config::WriteSetting(const QString& name, const QVariant& value,
                          const QVariant& default_value) {
    qt_config->setValue(name + "/default", value == default_value);
    qt_config->setValue(name, value);
}

void Config::Reload() {
    ReadValues();
    // To apply default value changes
    SaveValues();
    Settings::Apply();
}

void Config::Save() {
    SaveValues();
}

Config::~Config() {
    Save();

    delete qt_config;
}
