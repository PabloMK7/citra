// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <unordered_map>
#include <QKeySequence>
#include <QSettings>
#include "citra_qt/configuration/config.h"
#include "citra_qt/uisettings.h"
#include "common/file_util.h"
#include "core/frontend/mic.h"
#include "core/hle/service/service.h"
#include "input_common/main.h"
#include "input_common/udp/client.h"
#include "network/network.h"

Config::Config() {
    // TODO: Don't hardcode the path; let the frontend decide where to put the config files.
    qt_config_loc = FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "qt-config.ini";
    FileUtil::CreateFullPath(qt_config_loc);
    qt_config =
        std::make_unique<QSettings>(QString::fromStdString(qt_config_loc), QSettings::IniFormat);
    Reload();
}

Config::~Config() {
    Save();
}

const std::array<int, Settings::NativeButton::NumButtons> Config::default_buttons = {
    Qt::Key_A, Qt::Key_S, Qt::Key_Z, Qt::Key_X, Qt::Key_T, Qt::Key_G,
    Qt::Key_F, Qt::Key_H, Qt::Key_Q, Qt::Key_W, Qt::Key_M, Qt::Key_N,
    Qt::Key_O, Qt::Key_P, Qt::Key_1, Qt::Key_2, Qt::Key_B,
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

// This shouldn't have anything except static initializers (no functions). So
// QKeySequence(...).toString() is NOT ALLOWED HERE.
// This must be in alphabetical order according to action name as it must have the same order as
// UISetting::values.shortcuts, which is alphabetically ordered.
// clang-format off
const std::array<UISettings::Shortcut, 20> default_hotkeys{
    {{QStringLiteral("Advance Frame"),            QStringLiteral("Main Window"), {QStringLiteral("\\"), Qt::ApplicationShortcut}},
     {QStringLiteral("Capture Screenshot"),       QStringLiteral("Main Window"), {QStringLiteral("Ctrl+P"), Qt::ApplicationShortcut}},
     {QStringLiteral("Continue/Pause Emulation"), QStringLiteral("Main Window"), {QStringLiteral("F4"), Qt::WindowShortcut}},
     {QStringLiteral("Decrease Speed Limit"),     QStringLiteral("Main Window"), {QStringLiteral("-"), Qt::ApplicationShortcut}},
     {QStringLiteral("Exit Citra"),               QStringLiteral("Main Window"), {QStringLiteral("Ctrl+Q"), Qt::WindowShortcut}},
     {QStringLiteral("Exit Fullscreen"),          QStringLiteral("Main Window"), {QStringLiteral("Esc"), Qt::WindowShortcut}},
     {QStringLiteral("Fullscreen"),               QStringLiteral("Main Window"), {QStringLiteral("F11"), Qt::WindowShortcut}},
     {QStringLiteral("Increase Speed Limit"),     QStringLiteral("Main Window"), {QStringLiteral("+"), Qt::ApplicationShortcut}},
     {QStringLiteral("Load Amiibo"),              QStringLiteral("Main Window"), {QStringLiteral("F2"), Qt::ApplicationShortcut}},
     {QStringLiteral("Load File"),                QStringLiteral("Main Window"), {QStringLiteral("Ctrl+O"), Qt::WindowShortcut}},
     {QStringLiteral("Remove Amiibo"),            QStringLiteral("Main Window"), {QStringLiteral("F3"), Qt::ApplicationShortcut}},
     {QStringLiteral("Restart Emulation"),        QStringLiteral("Main Window"), {QStringLiteral("F6"), Qt::WindowShortcut}},
     {QStringLiteral("Stop Emulation"),           QStringLiteral("Main Window"), {QStringLiteral("F5"), Qt::WindowShortcut}},
     {QStringLiteral("Swap Screens"),             QStringLiteral("Main Window"), {QStringLiteral("F9"), Qt::WindowShortcut}},
     {QStringLiteral("Toggle Filter Bar"),        QStringLiteral("Main Window"), {QStringLiteral("Ctrl+F"), Qt::WindowShortcut}},
     {QStringLiteral("Toggle Frame Advancing"),   QStringLiteral("Main Window"), {QStringLiteral("Ctrl+A"), Qt::ApplicationShortcut}},
     {QStringLiteral("Toggle Screen Layout"),     QStringLiteral("Main Window"), {QStringLiteral("F10"), Qt::WindowShortcut}},
     {QStringLiteral("Toggle Speed Limit"),       QStringLiteral("Main Window"), {QStringLiteral("Ctrl+Z"), Qt::ApplicationShortcut}},
     {QStringLiteral("Toggle Status Bar"),        QStringLiteral("Main Window"), {QStringLiteral("Ctrl+S"), Qt::WindowShortcut}},
     {QStringLiteral("Toggle Texture Dumping"),   QStringLiteral("Main Window"), {QStringLiteral("Ctrl+D"), Qt::ApplicationShortcut}}}};
// clang-format on

void Config::ReadValues() {
    ReadControlValues();
    ReadCoreValues();
    ReadRendererValues();
    ReadLayoutValues();
    ReadAudioValues();
    ReadCameraValues();
    ReadDataStorageValues();
    ReadSystemValues();
    ReadMiscellaneousValues();
    ReadDebuggingValues();
    ReadWebServiceValues();
    ReadUIValues();
    ReadUtilityValues();
}

void Config::ReadAudioValues() {
    qt_config->beginGroup(QStringLiteral("Audio"));

    Settings::values.enable_dsp_lle = ReadSetting(QStringLiteral("enable_dsp_lle"), false).toBool();
    Settings::values.enable_dsp_lle_multithread =
        ReadSetting(QStringLiteral("enable_dsp_lle_multithread"), false).toBool();
    Settings::values.sink_id = ReadSetting(QStringLiteral("output_engine"), QStringLiteral("auto"))
                                   .toString()
                                   .toStdString();
    Settings::values.enable_audio_stretching =
        ReadSetting(QStringLiteral("enable_audio_stretching"), true).toBool();
    Settings::values.audio_device_id =
        ReadSetting(QStringLiteral("output_device"), QStringLiteral("auto"))
            .toString()
            .toStdString();
    Settings::values.volume = ReadSetting(QStringLiteral("volume"), 1).toFloat();
    Settings::values.mic_input_type = static_cast<Settings::MicInputType>(
        ReadSetting(QStringLiteral("mic_input_type"), 0).toInt());
    Settings::values.mic_input_device =
        ReadSetting(QStringLiteral("mic_input_device"), Frontend::Mic::default_device_name)
            .toString()
            .toStdString();

    qt_config->endGroup();
}

void Config::ReadCameraValues() {
    using namespace Service::CAM;
    qt_config->beginGroup(QStringLiteral("Camera"));

    Settings::values.camera_name[OuterRightCamera] =
        ReadSetting(QStringLiteral("camera_outer_right_name"), QStringLiteral("blank"))
            .toString()
            .toStdString();
    Settings::values.camera_config[OuterRightCamera] =
        ReadSetting(QStringLiteral("camera_outer_right_config"), QString{})
            .toString()
            .toStdString();
    Settings::values.camera_flip[OuterRightCamera] =
        ReadSetting(QStringLiteral("camera_outer_right_flip"), 0).toInt();
    Settings::values.camera_name[InnerCamera] =
        ReadSetting(QStringLiteral("camera_inner_name"), QStringLiteral("blank"))
            .toString()
            .toStdString();
    Settings::values.camera_config[InnerCamera] =
        ReadSetting(QStringLiteral("camera_inner_config"), QString{}).toString().toStdString();
    Settings::values.camera_flip[InnerCamera] =
        ReadSetting(QStringLiteral("camera_inner_flip"), 0).toInt();
    Settings::values.camera_name[OuterLeftCamera] =
        ReadSetting(QStringLiteral("camera_outer_left_name"), QStringLiteral("blank"))
            .toString()
            .toStdString();
    Settings::values.camera_config[OuterLeftCamera] =
        ReadSetting(QStringLiteral("camera_outer_left_config"), QString{}).toString().toStdString();
    Settings::values.camera_flip[OuterLeftCamera] =
        ReadSetting(QStringLiteral("camera_outer_left_flip"), 0).toInt();

    qt_config->endGroup();
}

void Config::ReadControlValues() {
    qt_config->beginGroup(QStringLiteral("Controls"));

    Settings::values.current_input_profile_index =
        ReadSetting(QStringLiteral("profile"), 0).toInt();

    const auto append_profile = [this] {
        Settings::InputProfile profile;
        profile.name =
            ReadSetting(QStringLiteral("name"), QStringLiteral("default")).toString().toStdString();
        for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
            std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
            profile.buttons[i] = ReadSetting(QString::fromUtf8(Settings::NativeButton::mapping[i]),
                                             QString::fromStdString(default_param))
                                     .toString()
                                     .toStdString();
            if (profile.buttons[i].empty())
                profile.buttons[i] = default_param;
        }
        for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
            std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
                default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
                default_analogs[i][3], default_analogs[i][4], 0.5f);
            profile.analogs[i] = ReadSetting(QString::fromUtf8(Settings::NativeAnalog::mapping[i]),
                                             QString::fromStdString(default_param))
                                     .toString()
                                     .toStdString();
            if (profile.analogs[i].empty())
                profile.analogs[i] = default_param;
        }
        profile.motion_device =
            ReadSetting(QStringLiteral("motion_device"),
                        QStringLiteral(
                            "engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0"))
                .toString()
                .toStdString();
        profile.touch_device =
            ReadSetting(QStringLiteral("touch_device"), QStringLiteral("engine:emu_window"))
                .toString()
                .toStdString();
        profile.udp_input_address =
            ReadSetting(QStringLiteral("udp_input_address"),
                        QString::fromUtf8(InputCommon::CemuhookUDP::DEFAULT_ADDR))
                .toString()
                .toStdString();
        profile.udp_input_port = static_cast<u16>(
            ReadSetting(QStringLiteral("udp_input_port"), InputCommon::CemuhookUDP::DEFAULT_PORT)
                .toInt());
        profile.udp_pad_index =
            static_cast<u8>(ReadSetting(QStringLiteral("udp_pad_index"), 0).toUInt());
        Settings::values.input_profiles.emplace_back(std::move(profile));
    };

    int num_input_profiles = qt_config->beginReadArray(QStringLiteral("profiles"));

    for (int i = 0; i < num_input_profiles; ++i) {
        qt_config->setArrayIndex(i);
        append_profile();
    }

    qt_config->endArray();

    // create a input profile if no input profiles exist, with the default or old settings
    if (num_input_profiles == 0) {
        append_profile();
        num_input_profiles = 1;
    }

    // ensure that the current input profile index is valid.
    Settings::values.current_input_profile_index =
        std::clamp(Settings::values.current_input_profile_index, 0, num_input_profiles - 1);

    Settings::LoadProfile(Settings::values.current_input_profile_index);

    qt_config->endGroup();
}

void Config::ReadUtilityValues() {
    qt_config->beginGroup("Utility");

    Settings::values.dump_textures = ReadSetting("dump_textures", false).toBool();
    Settings::values.custom_textures = ReadSetting("custom_textures", false).toBool();
    Settings::values.preload_textures = ReadSetting("preload_textures", false).toBool();

    qt_config->endGroup();
}

void Config::ReadCoreValues() {
    qt_config->beginGroup(QStringLiteral("Core"));

    Settings::values.use_cpu_jit = ReadSetting(QStringLiteral("use_cpu_jit"), true).toBool();

    qt_config->endGroup();
}

void Config::ReadDataStorageValues() {
    qt_config->beginGroup(QStringLiteral("Data Storage"));

    Settings::values.use_virtual_sd = ReadSetting(QStringLiteral("use_virtual_sd"), true).toBool();

    qt_config->endGroup();
}

void Config::ReadDebuggingValues() {
    qt_config->beginGroup(QStringLiteral("Debugging"));

    // Intentionally not using the QT default setting as this is intended to be changed in the ini
    Settings::values.record_frame_times =
        qt_config->value(QStringLiteral("record_frame_times"), false).toBool();
    Settings::values.use_gdbstub = ReadSetting(QStringLiteral("use_gdbstub"), false).toBool();
    Settings::values.gdbstub_port = ReadSetting(QStringLiteral("gdbstub_port"), 24689).toInt();

    qt_config->beginGroup(QStringLiteral("LLE"));
    for (const auto& service_module : Service::service_module_map) {
        bool use_lle = ReadSetting(QString::fromStdString(service_module.name), false).toBool();
        Settings::values.lle_modules.emplace(service_module.name, use_lle);
    }
    qt_config->endGroup();

    qt_config->endGroup();
}

void Config::ReadLayoutValues() {
    qt_config->beginGroup(QStringLiteral("Layout"));

    Settings::values.render_3d = static_cast<Settings::StereoRenderOption>(
        ReadSetting(QStringLiteral("render_3d"), 0).toInt());
    Settings::values.factor_3d = ReadSetting(QStringLiteral("factor_3d"), 0).toInt();
    Settings::values.pp_shader_name =
        ReadSetting(QStringLiteral("pp_shader_name"),
                    (Settings::values.render_3d == Settings::StereoRenderOption::Anaglyph)
                        ? QStringLiteral("dubois (builtin)")
                        : QStringLiteral("none (builtin)"))
            .toString()
            .toStdString();
    Settings::values.filter_mode = ReadSetting(QStringLiteral("filter_mode"), true).toBool();
    Settings::values.layout_option =
        static_cast<Settings::LayoutOption>(ReadSetting(QStringLiteral("layout_option")).toInt());
    Settings::values.swap_screen = ReadSetting(QStringLiteral("swap_screen"), false).toBool();
    Settings::values.custom_layout = ReadSetting(QStringLiteral("custom_layout"), false).toBool();
    Settings::values.custom_top_left = ReadSetting(QStringLiteral("custom_top_left"), 0).toInt();
    Settings::values.custom_top_top = ReadSetting(QStringLiteral("custom_top_top"), 0).toInt();
    Settings::values.custom_top_right =
        ReadSetting(QStringLiteral("custom_top_right"), 400).toInt();
    Settings::values.custom_top_bottom =
        ReadSetting(QStringLiteral("custom_top_bottom"), 240).toInt();
    Settings::values.custom_bottom_left =
        ReadSetting(QStringLiteral("custom_bottom_left"), 40).toInt();
    Settings::values.custom_bottom_top =
        ReadSetting(QStringLiteral("custom_bottom_top"), 240).toInt();
    Settings::values.custom_bottom_right =
        ReadSetting(QStringLiteral("custom_bottom_right"), 360).toInt();
    Settings::values.custom_bottom_bottom =
        ReadSetting(QStringLiteral("custom_bottom_bottom"), 480).toInt();

    qt_config->endGroup();
}

void Config::ReadMiscellaneousValues() {
    qt_config->beginGroup(QStringLiteral("Miscellaneous"));

    Settings::values.log_filter =
        ReadSetting(QStringLiteral("log_filter"), QStringLiteral("*:Info"))
            .toString()
            .toStdString();

    qt_config->endGroup();
}

void Config::ReadMultiplayerValues() {
    qt_config->beginGroup(QStringLiteral("Multiplayer"));

    UISettings::values.nickname = ReadSetting(QStringLiteral("nickname"), QString{}).toString();
    UISettings::values.ip = ReadSetting(QStringLiteral("ip"), QString{}).toString();
    UISettings::values.port =
        ReadSetting(QStringLiteral("port"), Network::DefaultRoomPort).toString();
    UISettings::values.room_nickname =
        ReadSetting(QStringLiteral("room_nickname"), QString{}).toString();
    UISettings::values.room_name = ReadSetting(QStringLiteral("room_name"), QString{}).toString();
    UISettings::values.room_port =
        ReadSetting(QStringLiteral("room_port"), QStringLiteral("24872")).toString();
    bool ok;
    UISettings::values.host_type = ReadSetting(QStringLiteral("host_type"), 0).toUInt(&ok);
    if (!ok) {
        UISettings::values.host_type = 0;
    }
    UISettings::values.max_player = ReadSetting(QStringLiteral("max_player"), 8).toUInt();
    UISettings::values.game_id = ReadSetting(QStringLiteral("game_id"), 0).toULongLong();
    UISettings::values.room_description =
        ReadSetting(QStringLiteral("room_description"), QString{}).toString();
    // Read ban list back
    int size = qt_config->beginReadArray(QStringLiteral("username_ban_list"));
    UISettings::values.ban_list.first.resize(size);
    for (int i = 0; i < size; ++i) {
        qt_config->setArrayIndex(i);
        UISettings::values.ban_list.first[i] =
            ReadSetting(QStringLiteral("username")).toString().toStdString();
    }
    qt_config->endArray();
    size = qt_config->beginReadArray(QStringLiteral("ip_ban_list"));
    UISettings::values.ban_list.second.resize(size);
    for (int i = 0; i < size; ++i) {
        qt_config->setArrayIndex(i);
        UISettings::values.ban_list.second[i] =
            ReadSetting(QStringLiteral("ip")).toString().toStdString();
    }
    qt_config->endArray();

    qt_config->endGroup();
}

void Config::ReadPathValues() {
    qt_config->beginGroup(QStringLiteral("Paths"));

    UISettings::values.roms_path = ReadSetting(QStringLiteral("romsPath")).toString();
    UISettings::values.symbols_path = ReadSetting(QStringLiteral("symbolsPath")).toString();
    UISettings::values.movie_record_path =
        ReadSetting(QStringLiteral("movieRecordPath")).toString();
    UISettings::values.movie_playback_path =
        ReadSetting(QStringLiteral("moviePlaybackPath")).toString();
    UISettings::values.screenshot_path = ReadSetting(QStringLiteral("screenshotPath")).toString();
    UISettings::values.video_dumping_path =
        ReadSetting(QStringLiteral("videoDumpingPath")).toString();
    UISettings::values.game_dir_deprecated =
        ReadSetting(QStringLiteral("gameListRootDir"), QStringLiteral(".")).toString();
    UISettings::values.game_dir_deprecated_deepscan =
        ReadSetting(QStringLiteral("gameListDeepScan"), false).toBool();
    int size = qt_config->beginReadArray(QStringLiteral("gamedirs"));
    for (int i = 0; i < size; ++i) {
        qt_config->setArrayIndex(i);
        UISettings::GameDir game_dir;
        game_dir.path = ReadSetting(QStringLiteral("path")).toString();
        game_dir.deep_scan = ReadSetting(QStringLiteral("deep_scan"), false).toBool();
        game_dir.expanded = ReadSetting(QStringLiteral("expanded"), true).toBool();
        UISettings::values.game_dirs.append(game_dir);
    }
    qt_config->endArray();
    // create NAND and SD card directories if empty, these are not removable through the UI,
    // also carries over old game list settings if present
    if (UISettings::values.game_dirs.isEmpty()) {
        UISettings::GameDir game_dir;
        game_dir.path = QStringLiteral("INSTALLED");
        game_dir.expanded = true;
        UISettings::values.game_dirs.append(game_dir);
        game_dir.path = QStringLiteral("SYSTEM");
        UISettings::values.game_dirs.append(game_dir);
        if (UISettings::values.game_dir_deprecated != QStringLiteral(".")) {
            game_dir.path = UISettings::values.game_dir_deprecated;
            game_dir.deep_scan = UISettings::values.game_dir_deprecated_deepscan;
            UISettings::values.game_dirs.append(game_dir);
        }
    }
    UISettings::values.recent_files = ReadSetting(QStringLiteral("recentFiles")).toStringList();
    UISettings::values.language = ReadSetting(QStringLiteral("language"), QString{}).toString();

    qt_config->endGroup();
}

void Config::ReadRendererValues() {
    qt_config->beginGroup(QStringLiteral("Renderer"));

    Settings::values.use_hw_renderer =
        ReadSetting(QStringLiteral("use_hw_renderer"), true).toBool();
#ifdef __APPLE__
    // Hardware shader is broken on macos thanks to poor drivers.
    // We still want to provide this option for test/development purposes, but disable it by
    // default.
    Settings::values.use_hw_shader = ReadSetting(QStringLiteral("use_hw_shader"), false).toBool();
#else
    Settings::values.use_hw_shader = ReadSetting(QStringLiteral("use_hw_shader"), true).toBool();
#endif
    Settings::values.shaders_accurate_mul =
        ReadSetting(QStringLiteral("shaders_accurate_mul"), false).toBool();
    Settings::values.use_shader_jit = ReadSetting(QStringLiteral("use_shader_jit"), true).toBool();
    Settings::values.resolution_factor =
        static_cast<u16>(ReadSetting(QStringLiteral("resolution_factor"), 1).toInt());
    Settings::values.vsync_enabled = ReadSetting(QStringLiteral("vsync_enabled"), false).toBool();
    Settings::values.use_frame_limit =
        ReadSetting(QStringLiteral("use_frame_limit"), true).toBool();
    Settings::values.frame_limit = ReadSetting(QStringLiteral("frame_limit"), 100).toInt();

    Settings::values.bg_red = ReadSetting(QStringLiteral("bg_red"), 0.0).toFloat();
    Settings::values.bg_green = ReadSetting(QStringLiteral("bg_green"), 0.0).toFloat();
    Settings::values.bg_blue = ReadSetting(QStringLiteral("bg_blue"), 0.0).toFloat();

    qt_config->endGroup();
}

void Config::ReadShortcutValues() {
    qt_config->beginGroup(QStringLiteral("Shortcuts"));

    for (auto [name, group, shortcut] : default_hotkeys) {
        auto [keyseq, context] = shortcut;
        qt_config->beginGroup(group);
        qt_config->beginGroup(name);
        UISettings::values.shortcuts.push_back(
            {name,
             group,
             {ReadSetting(QStringLiteral("KeySeq"), keyseq).toString(),
              ReadSetting(QStringLiteral("Context"), context).toInt()}});
        qt_config->endGroup();
        qt_config->endGroup();
    }

    qt_config->endGroup();
}

void Config::ReadSystemValues() {
    qt_config->beginGroup(QStringLiteral("System"));

    Settings::values.is_new_3ds = ReadSetting(QStringLiteral("is_new_3ds"), false).toBool();
    Settings::values.region_value =
        ReadSetting(QStringLiteral("region_value"), Settings::REGION_VALUE_AUTO_SELECT).toInt();
    Settings::values.init_clock = static_cast<Settings::InitClock>(
        ReadSetting(QStringLiteral("init_clock"), static_cast<u32>(Settings::InitClock::SystemTime))
            .toInt());
    Settings::values.init_time =
        ReadSetting(QStringLiteral("init_time"), 946681277ULL).toULongLong();

    qt_config->endGroup();
}

void Config::ReadUIValues() {
    qt_config->beginGroup(QStringLiteral("UI"));

    UISettings::values.theme =
        ReadSetting(QStringLiteral("theme"), QString::fromUtf8(UISettings::themes[0].second))
            .toString();
    UISettings::values.enable_discord_presence =
        ReadSetting(QStringLiteral("enable_discord_presence"), true).toBool();
    UISettings::values.screenshot_resolution_factor =
        static_cast<u16>(ReadSetting(QStringLiteral("screenshot_resolution_factor"), 0).toUInt());

    ReadUpdaterValues();
    ReadUILayoutValues();
    ReadUIGameListValues();
    ReadPathValues();
    ReadShortcutValues();
    ReadMultiplayerValues();

    UISettings::values.single_window_mode =
        ReadSetting(QStringLiteral("singleWindowMode"), true).toBool();
    UISettings::values.fullscreen = ReadSetting(QStringLiteral("fullscreen"), false).toBool();
    UISettings::values.display_titlebar =
        ReadSetting(QStringLiteral("displayTitleBars"), true).toBool();
    UISettings::values.show_filter_bar =
        ReadSetting(QStringLiteral("showFilterBar"), true).toBool();
    UISettings::values.show_status_bar =
        ReadSetting(QStringLiteral("showStatusBar"), true).toBool();
    UISettings::values.confirm_before_closing =
        ReadSetting(QStringLiteral("confirmClose"), true).toBool();
    UISettings::values.first_start = ReadSetting(QStringLiteral("firstStart"), true).toBool();
    UISettings::values.callout_flags = ReadSetting(QStringLiteral("calloutFlags"), 0).toUInt();
    UISettings::values.show_console = ReadSetting(QStringLiteral("showConsole"), false).toBool();
    UISettings::values.pause_when_in_background =
        ReadSetting(QStringLiteral("pauseWhenInBackground"), false).toBool();

    qt_config->endGroup();
}

void Config::ReadUIGameListValues() {
    qt_config->beginGroup(QStringLiteral("GameList"));

    auto icon_size = UISettings::GameListIconSize{
        ReadSetting(QStringLiteral("iconSize"),
                    static_cast<int>(UISettings::GameListIconSize::LargeIcon))
            .toInt()};
    if (icon_size < UISettings::GameListIconSize::NoIcon ||
        icon_size > UISettings::GameListIconSize::LargeIcon) {
        icon_size = UISettings::GameListIconSize::LargeIcon;
    }
    UISettings::values.game_list_icon_size = icon_size;

    UISettings::GameListText row_1 = UISettings::GameListText{
        ReadSetting(QStringLiteral("row1"), static_cast<int>(UISettings::GameListText::TitleName))
            .toInt()};
    if (row_1 <= UISettings::GameListText::NoText || row_1 >= UISettings::GameListText::ListEnd) {
        row_1 = UISettings::GameListText::TitleName;
    }
    UISettings::values.game_list_row_1 = row_1;

    UISettings::GameListText row_2 = UISettings::GameListText{
        ReadSetting(QStringLiteral("row2"), static_cast<int>(UISettings::GameListText::FileName))
            .toInt()};
    if (row_2 < UISettings::GameListText::NoText || row_2 >= UISettings::GameListText::ListEnd) {
        row_2 = UISettings::GameListText::FileName;
    }
    UISettings::values.game_list_row_2 = row_2;

    UISettings::values.game_list_hide_no_icon =
        ReadSetting(QStringLiteral("hideNoIcon"), false).toBool();
    UISettings::values.game_list_single_line_mode =
        ReadSetting(QStringLiteral("singleLineMode"), false).toBool();

    qt_config->endGroup();
}

void Config::ReadUILayoutValues() {
    qt_config->beginGroup(QStringLiteral("UILayout"));

    UISettings::values.geometry = ReadSetting(QStringLiteral("geometry")).toByteArray();
    UISettings::values.state = ReadSetting(QStringLiteral("state")).toByteArray();
    UISettings::values.renderwindow_geometry =
        ReadSetting(QStringLiteral("geometryRenderWindow")).toByteArray();
    UISettings::values.gamelist_header_state =
        ReadSetting(QStringLiteral("gameListHeaderState")).toByteArray();
    UISettings::values.microprofile_geometry =
        ReadSetting(QStringLiteral("microProfileDialogGeometry")).toByteArray();
    UISettings::values.microprofile_visible =
        ReadSetting(QStringLiteral("microProfileDialogVisible"), false).toBool();

    qt_config->endGroup();
}

void Config::ReadUpdaterValues() {
    qt_config->beginGroup(QStringLiteral("Updater"));

    UISettings::values.check_for_update_on_start =
        ReadSetting(QStringLiteral("check_for_update_on_start"), true).toBool();
    UISettings::values.update_on_close =
        ReadSetting(QStringLiteral("update_on_close"), false).toBool();

    qt_config->endGroup();
}

void Config::ReadWebServiceValues() {
    qt_config->beginGroup(QStringLiteral("WebService"));

    Settings::values.enable_telemetry =
        ReadSetting(QStringLiteral("enable_telemetry"), true).toBool();
    Settings::values.web_api_url =
        ReadSetting(QStringLiteral("web_api_url"), QStringLiteral("https://api.citra-emu.org"))
            .toString()
            .toStdString();
    Settings::values.citra_username =
        ReadSetting(QStringLiteral("citra_username")).toString().toStdString();
    Settings::values.citra_token =
        ReadSetting(QStringLiteral("citra_token")).toString().toStdString();

    qt_config->endGroup();
}

void Config::SaveValues() {
    SaveControlValues();
    SaveCoreValues();
    SaveRendererValues();
    SaveLayoutValues();
    SaveAudioValues();
    SaveCameraValues();
    SaveDataStorageValues();
    SaveSystemValues();
    SaveMiscellaneousValues();
    SaveDebuggingValues();
    SaveWebServiceValues();
    SaveUIValues();
    SaveUtilityValues();
}

void Config::SaveAudioValues() {
    qt_config->beginGroup(QStringLiteral("Audio"));

    WriteSetting(QStringLiteral("enable_dsp_lle"), Settings::values.enable_dsp_lle, false);
    WriteSetting(QStringLiteral("enable_dsp_lle_multithread"),
                 Settings::values.enable_dsp_lle_multithread, false);
    WriteSetting(QStringLiteral("output_engine"), QString::fromStdString(Settings::values.sink_id),
                 QStringLiteral("auto"));
    WriteSetting(QStringLiteral("enable_audio_stretching"),
                 Settings::values.enable_audio_stretching, true);
    WriteSetting(QStringLiteral("output_device"),
                 QString::fromStdString(Settings::values.audio_device_id), QStringLiteral("auto"));
    WriteSetting(QStringLiteral("volume"), Settings::values.volume, 1.0f);
    WriteSetting(QStringLiteral("mic_input_device"),
                 QString::fromStdString(Settings::values.mic_input_device),
                 Frontend::Mic::default_device_name);
    WriteSetting(QStringLiteral("mic_input_type"),
                 static_cast<int>(Settings::values.mic_input_type), 0);

    qt_config->endGroup();
}

void Config::SaveCameraValues() {
    using namespace Service::CAM;
    qt_config->beginGroup(QStringLiteral("Camera"));

    WriteSetting(QStringLiteral("camera_outer_right_name"),
                 QString::fromStdString(Settings::values.camera_name[OuterRightCamera]),
                 QStringLiteral("blank"));
    WriteSetting(QStringLiteral("camera_outer_right_config"),
                 QString::fromStdString(Settings::values.camera_config[OuterRightCamera]),
                 QString{});
    WriteSetting(QStringLiteral("camera_outer_right_flip"),
                 Settings::values.camera_flip[OuterRightCamera], 0);
    WriteSetting(QStringLiteral("camera_inner_name"),
                 QString::fromStdString(Settings::values.camera_name[InnerCamera]),
                 QStringLiteral("blank"));
    WriteSetting(QStringLiteral("camera_inner_config"),
                 QString::fromStdString(Settings::values.camera_config[InnerCamera]), QString{});
    WriteSetting(QStringLiteral("camera_inner_flip"), Settings::values.camera_flip[InnerCamera], 0);
    WriteSetting(QStringLiteral("camera_outer_left_name"),
                 QString::fromStdString(Settings::values.camera_name[OuterLeftCamera]),
                 QStringLiteral("blank"));
    WriteSetting(QStringLiteral("camera_outer_left_config"),
                 QString::fromStdString(Settings::values.camera_config[OuterLeftCamera]),
                 QString{});
    WriteSetting(QStringLiteral("camera_outer_left_flip"),
                 Settings::values.camera_flip[OuterLeftCamera], 0);

    qt_config->endGroup();
}

void Config::SaveControlValues() {
    qt_config->beginGroup(QStringLiteral("Controls"));

    WriteSetting(QStringLiteral("profile"), Settings::values.current_input_profile_index, 0);
    qt_config->beginWriteArray(QStringLiteral("profiles"));
    for (std::size_t p = 0; p < Settings::values.input_profiles.size(); ++p) {
        qt_config->setArrayIndex(static_cast<int>(p));
        const auto& profile = Settings::values.input_profiles[p];
        WriteSetting(QStringLiteral("name"), QString::fromStdString(profile.name),
                     QStringLiteral("default"));
        for (int i = 0; i < Settings::NativeButton::NumButtons; ++i) {
            std::string default_param = InputCommon::GenerateKeyboardParam(default_buttons[i]);
            WriteSetting(QString::fromStdString(Settings::NativeButton::mapping[i]),
                         QString::fromStdString(profile.buttons[i]),
                         QString::fromStdString(default_param));
        }
        for (int i = 0; i < Settings::NativeAnalog::NumAnalogs; ++i) {
            std::string default_param = InputCommon::GenerateAnalogParamFromKeys(
                default_analogs[i][0], default_analogs[i][1], default_analogs[i][2],
                default_analogs[i][3], default_analogs[i][4], 0.5f);
            WriteSetting(QString::fromStdString(Settings::NativeAnalog::mapping[i]),
                         QString::fromStdString(profile.analogs[i]),
                         QString::fromStdString(default_param));
        }
        WriteSetting(
            QStringLiteral("motion_device"), QString::fromStdString(profile.motion_device),
            QStringLiteral("engine:motion_emu,update_period:100,sensitivity:0.01,tilt_clamp:90.0"));
        WriteSetting(QStringLiteral("touch_device"), QString::fromStdString(profile.touch_device),
                     QStringLiteral("engine:emu_window"));
        WriteSetting(QStringLiteral("udp_input_address"),
                     QString::fromStdString(profile.udp_input_address),
                     QString::fromUtf8(InputCommon::CemuhookUDP::DEFAULT_ADDR));
        WriteSetting(QStringLiteral("udp_input_port"), profile.udp_input_port,
                     InputCommon::CemuhookUDP::DEFAULT_PORT);
        WriteSetting(QStringLiteral("udp_pad_index"), profile.udp_pad_index, 0);
    }
    qt_config->endArray();

    qt_config->endGroup();
}

void Config::SaveUtilityValues() {
    qt_config->beginGroup("Utility");

    WriteSetting("dump_textures", Settings::values.dump_textures, false);
    WriteSetting("custom_textures", Settings::values.custom_textures, false);
    WriteSetting("preload_textures", Settings::values.preload_textures, false);

    qt_config->endGroup();
}

void Config::SaveCoreValues() {
    qt_config->beginGroup(QStringLiteral("Core"));

    WriteSetting(QStringLiteral("use_cpu_jit"), Settings::values.use_cpu_jit, true);

    qt_config->endGroup();
}

void Config::SaveDataStorageValues() {
    qt_config->beginGroup(QStringLiteral("Data Storage"));

    WriteSetting(QStringLiteral("use_virtual_sd"), Settings::values.use_virtual_sd, true);

    qt_config->endGroup();
}

void Config::SaveDebuggingValues() {
    qt_config->beginGroup(QStringLiteral("Debugging"));

    // Intentionally not using the QT default setting as this is intended to be changed in the ini
    qt_config->setValue(QStringLiteral("record_frame_times"), Settings::values.record_frame_times);
    WriteSetting(QStringLiteral("use_gdbstub"), Settings::values.use_gdbstub, false);
    WriteSetting(QStringLiteral("gdbstub_port"), Settings::values.gdbstub_port, 24689);

    qt_config->beginGroup(QStringLiteral("LLE"));
    for (const auto& service_module : Settings::values.lle_modules) {
        WriteSetting(QString::fromStdString(service_module.first), service_module.second, false);
    }
    qt_config->endGroup();

    qt_config->endGroup();
}

void Config::SaveLayoutValues() {
    qt_config->beginGroup(QStringLiteral("Layout"));

    WriteSetting(QStringLiteral("render_3d"), static_cast<int>(Settings::values.render_3d), 0);
    WriteSetting(QStringLiteral("factor_3d"), Settings::values.factor_3d.load(), 0);
    WriteSetting(QStringLiteral("pp_shader_name"),
                 QString::fromStdString(Settings::values.pp_shader_name),
                 (Settings::values.render_3d == Settings::StereoRenderOption::Anaglyph)
                     ? QStringLiteral("dubois (builtin)")
                     : QStringLiteral("none (builtin)"));
    WriteSetting(QStringLiteral("filter_mode"), Settings::values.filter_mode, true);
    WriteSetting(QStringLiteral("layout_option"), static_cast<int>(Settings::values.layout_option));
    WriteSetting(QStringLiteral("swap_screen"), Settings::values.swap_screen, false);
    WriteSetting(QStringLiteral("custom_layout"), Settings::values.custom_layout, false);
    WriteSetting(QStringLiteral("custom_top_left"), Settings::values.custom_top_left, 0);
    WriteSetting(QStringLiteral("custom_top_top"), Settings::values.custom_top_top, 0);
    WriteSetting(QStringLiteral("custom_top_right"), Settings::values.custom_top_right, 400);
    WriteSetting(QStringLiteral("custom_top_bottom"), Settings::values.custom_top_bottom, 240);
    WriteSetting(QStringLiteral("custom_bottom_left"), Settings::values.custom_bottom_left, 40);
    WriteSetting(QStringLiteral("custom_bottom_top"), Settings::values.custom_bottom_top, 240);
    WriteSetting(QStringLiteral("custom_bottom_right"), Settings::values.custom_bottom_right, 360);
    WriteSetting(QStringLiteral("custom_bottom_bottom"), Settings::values.custom_bottom_bottom,
                 480);

    qt_config->endGroup();
}

void Config::SaveMiscellaneousValues() {
    qt_config->beginGroup(QStringLiteral("Miscellaneous"));

    WriteSetting(QStringLiteral("log_filter"), QString::fromStdString(Settings::values.log_filter),
                 QStringLiteral("*:Info"));

    qt_config->endGroup();
}

void Config::SaveMultiplayerValues() {
    qt_config->beginGroup(QStringLiteral("Multiplayer"));

    WriteSetting(QStringLiteral("nickname"), UISettings::values.nickname, QString{});
    WriteSetting(QStringLiteral("ip"), UISettings::values.ip, QString{});
    WriteSetting(QStringLiteral("port"), UISettings::values.port, Network::DefaultRoomPort);
    WriteSetting(QStringLiteral("room_nickname"), UISettings::values.room_nickname, QString{});
    WriteSetting(QStringLiteral("room_name"), UISettings::values.room_name, QString{});
    WriteSetting(QStringLiteral("room_port"), UISettings::values.room_port,
                 QStringLiteral("24872"));
    WriteSetting(QStringLiteral("host_type"), UISettings::values.host_type, 0);
    WriteSetting(QStringLiteral("max_player"), UISettings::values.max_player, 8);
    WriteSetting(QStringLiteral("game_id"), UISettings::values.game_id, 0);
    WriteSetting(QStringLiteral("room_description"), UISettings::values.room_description,
                 QString{});
    // Write ban list
    qt_config->beginWriteArray(QStringLiteral("username_ban_list"));
    for (std::size_t i = 0; i < UISettings::values.ban_list.first.size(); ++i) {
        qt_config->setArrayIndex(i);
        WriteSetting(QStringLiteral("username"),
                     QString::fromStdString(UISettings::values.ban_list.first[i]));
    }
    qt_config->endArray();
    qt_config->beginWriteArray(QStringLiteral("ip_ban_list"));
    for (std::size_t i = 0; i < UISettings::values.ban_list.second.size(); ++i) {
        qt_config->setArrayIndex(i);
        WriteSetting(QStringLiteral("ip"),
                     QString::fromStdString(UISettings::values.ban_list.second[i]));
    }
    qt_config->endArray();

    qt_config->endGroup();
}

void Config::SavePathValues() {
    qt_config->beginGroup(QStringLiteral("Paths"));

    WriteSetting(QStringLiteral("romsPath"), UISettings::values.roms_path);
    WriteSetting(QStringLiteral("symbolsPath"), UISettings::values.symbols_path);
    WriteSetting(QStringLiteral("movieRecordPath"), UISettings::values.movie_record_path);
    WriteSetting(QStringLiteral("moviePlaybackPath"), UISettings::values.movie_playback_path);
    WriteSetting(QStringLiteral("screenshotPath"), UISettings::values.screenshot_path);
    WriteSetting(QStringLiteral("videoDumpingPath"), UISettings::values.video_dumping_path);
    qt_config->beginWriteArray(QStringLiteral("gamedirs"));
    for (int i = 0; i < UISettings::values.game_dirs.size(); ++i) {
        qt_config->setArrayIndex(i);
        const auto& game_dir = UISettings::values.game_dirs[i];
        WriteSetting(QStringLiteral("path"), game_dir.path);
        WriteSetting(QStringLiteral("deep_scan"), game_dir.deep_scan, false);
        WriteSetting(QStringLiteral("expanded"), game_dir.expanded, true);
    }
    qt_config->endArray();
    WriteSetting(QStringLiteral("recentFiles"), UISettings::values.recent_files);
    WriteSetting(QStringLiteral("language"), UISettings::values.language, QString{});

    qt_config->endGroup();
}

void Config::SaveRendererValues() {
    qt_config->beginGroup(QStringLiteral("Renderer"));

    WriteSetting(QStringLiteral("use_hw_renderer"), Settings::values.use_hw_renderer, true);
#ifdef __APPLE__
    // Hardware shader is broken on macos thanks to poor drivers.
    // We still want to provide this option for test/development purposes, but disable it by
    // default.
    WriteSetting(QStringLiteral("use_hw_shader"), Settings::values.use_hw_shader, false);
#else
    WriteSetting(QStringLiteral("use_hw_shader"), Settings::values.use_hw_shader, true);
#endif
    WriteSetting(QStringLiteral("shaders_accurate_mul"), Settings::values.shaders_accurate_mul,
                 false);
    WriteSetting(QStringLiteral("use_shader_jit"), Settings::values.use_shader_jit, true);
    WriteSetting(QStringLiteral("resolution_factor"), Settings::values.resolution_factor, 1);
    WriteSetting(QStringLiteral("vsync_enabled"), Settings::values.vsync_enabled, false);
    WriteSetting(QStringLiteral("use_frame_limit"), Settings::values.use_frame_limit, true);
    WriteSetting(QStringLiteral("frame_limit"), Settings::values.frame_limit, 100);

    // Cast to double because Qt's written float values are not human-readable
    WriteSetting(QStringLiteral("bg_red"), (double)Settings::values.bg_red, 0.0);
    WriteSetting(QStringLiteral("bg_green"), (double)Settings::values.bg_green, 0.0);
    WriteSetting(QStringLiteral("bg_blue"), (double)Settings::values.bg_blue, 0.0);

    qt_config->endGroup();
}

void Config::SaveShortcutValues() {
    qt_config->beginGroup(QStringLiteral("Shortcuts"));

    // Lengths of UISettings::values.shortcuts & default_hotkeys are same.
    // However, their ordering must also be the same.
    for (std::size_t i = 0; i < default_hotkeys.size(); i++) {
        auto [name, group, shortcut] = UISettings::values.shortcuts[i];
        qt_config->beginGroup(group);
        qt_config->beginGroup(name);
        WriteSetting(QStringLiteral("KeySeq"), shortcut.first, default_hotkeys[i].shortcut.first);
        WriteSetting(QStringLiteral("Context"), shortcut.second,
                     default_hotkeys[i].shortcut.second);
        qt_config->endGroup();
        qt_config->endGroup();
    }

    qt_config->endGroup();
}

void Config::SaveSystemValues() {
    qt_config->beginGroup(QStringLiteral("System"));

    WriteSetting(QStringLiteral("is_new_3ds"), Settings::values.is_new_3ds, false);
    WriteSetting(QStringLiteral("region_value"), Settings::values.region_value,
                 Settings::REGION_VALUE_AUTO_SELECT);
    WriteSetting(QStringLiteral("init_clock"), static_cast<u32>(Settings::values.init_clock),
                 static_cast<u32>(Settings::InitClock::SystemTime));
    WriteSetting(QStringLiteral("init_time"),
                 static_cast<unsigned long long>(Settings::values.init_time), 946681277ULL);

    qt_config->endGroup();
}

void Config::SaveUIValues() {
    qt_config->beginGroup(QStringLiteral("UI"));

    WriteSetting(QStringLiteral("theme"), UISettings::values.theme,
                 QString::fromUtf8(UISettings::themes[0].second));
    WriteSetting(QStringLiteral("enable_discord_presence"),
                 UISettings::values.enable_discord_presence, true);
    WriteSetting(QStringLiteral("screenshot_resolution_factor"),
                 UISettings::values.screenshot_resolution_factor, 0);

    SaveUpdaterValues();
    SaveUILayoutValues();
    SaveUIGameListValues();
    SavePathValues();
    SaveShortcutValues();
    SaveMultiplayerValues();

    WriteSetting(QStringLiteral("singleWindowMode"), UISettings::values.single_window_mode, true);
    WriteSetting(QStringLiteral("fullscreen"), UISettings::values.fullscreen, false);
    WriteSetting(QStringLiteral("displayTitleBars"), UISettings::values.display_titlebar, true);
    WriteSetting(QStringLiteral("showFilterBar"), UISettings::values.show_filter_bar, true);
    WriteSetting(QStringLiteral("showStatusBar"), UISettings::values.show_status_bar, true);
    WriteSetting(QStringLiteral("confirmClose"), UISettings::values.confirm_before_closing, true);
    WriteSetting(QStringLiteral("firstStart"), UISettings::values.first_start, true);
    WriteSetting(QStringLiteral("calloutFlags"), UISettings::values.callout_flags, 0);
    WriteSetting(QStringLiteral("showConsole"), UISettings::values.show_console, false);
    WriteSetting(QStringLiteral("pauseWhenInBackground"),
                 UISettings::values.pause_when_in_background, false);

    qt_config->endGroup();
}

void Config::SaveUIGameListValues() {
    qt_config->beginGroup(QStringLiteral("GameList"));

    WriteSetting(QStringLiteral("iconSize"),
                 static_cast<int>(UISettings::values.game_list_icon_size), 2);
    WriteSetting(QStringLiteral("row1"), static_cast<int>(UISettings::values.game_list_row_1), 2);
    WriteSetting(QStringLiteral("row2"), static_cast<int>(UISettings::values.game_list_row_2), 0);
    WriteSetting(QStringLiteral("hideNoIcon"), UISettings::values.game_list_hide_no_icon, false);
    WriteSetting(QStringLiteral("singleLineMode"), UISettings::values.game_list_single_line_mode,
                 false);

    qt_config->endGroup();
}

void Config::SaveUILayoutValues() {
    qt_config->beginGroup(QStringLiteral("UILayout"));

    WriteSetting(QStringLiteral("geometry"), UISettings::values.geometry);
    WriteSetting(QStringLiteral("state"), UISettings::values.state);
    WriteSetting(QStringLiteral("geometryRenderWindow"), UISettings::values.renderwindow_geometry);
    WriteSetting(QStringLiteral("gameListHeaderState"), UISettings::values.gamelist_header_state);
    WriteSetting(QStringLiteral("microProfileDialogGeometry"),
                 UISettings::values.microprofile_geometry);
    WriteSetting(QStringLiteral("microProfileDialogVisible"),
                 UISettings::values.microprofile_visible, false);

    qt_config->endGroup();
}

void Config::SaveUpdaterValues() {
    qt_config->beginGroup(QStringLiteral("Updater"));

    WriteSetting(QStringLiteral("check_for_update_on_start"),
                 UISettings::values.check_for_update_on_start, true);
    WriteSetting(QStringLiteral("update_on_close"), UISettings::values.update_on_close, false);

    qt_config->endGroup();
}

void Config::SaveWebServiceValues() {
    qt_config->beginGroup(QStringLiteral("WebService"));

    WriteSetting(QStringLiteral("enable_telemetry"), Settings::values.enable_telemetry, true);
    WriteSetting(QStringLiteral("web_api_url"),
                 QString::fromStdString(Settings::values.web_api_url),
                 QStringLiteral("https://api.citra-emu.org"));
    WriteSetting(QStringLiteral("citra_username"),
                 QString::fromStdString(Settings::values.citra_username));
    WriteSetting(QStringLiteral("citra_token"),
                 QString::fromStdString(Settings::values.citra_token));

    qt_config->endGroup();
}

QVariant Config::ReadSetting(const QString& name) const {
    return qt_config->value(name);
}

QVariant Config::ReadSetting(const QString& name, const QVariant& default_value) const {
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
