// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <utility>
#include <vector>
#include <QByteArray>
#include <QMetaType>
#include <QString>
#include <QStringList>
#include <QVector>
#include "common/common_types.h"

namespace UISettings {

using ContextualShortcut = std::pair<QString, int>;

struct Shortcut {
    QString name;
    QString group;
    ContextualShortcut shortcut;
};

using Themes = std::array<std::pair<const char*, const char*>, 4>;
extern const Themes themes;

struct GameDir {
    QString path;
    bool deep_scan = false;
    bool expanded = false;
    bool operator==(const GameDir& rhs) const {
        return path == rhs.path;
    }
    bool operator!=(const GameDir& rhs) const {
        return !operator==(rhs);
    }
};

enum class GameListIconSize {
    NoIcon,    ///< Do not display icons
    SmallIcon, ///< Display a small (24x24) icon
    LargeIcon, ///< Display a large (48x48) icon
};

enum class GameListText {
    NoText = -1,   ///< No text
    FileName,      ///< Display the file name of the entry
    FullPath,      ///< Display the full path of the entry
    TitleName,     ///< Display the name of the title
    TitleID,       ///< Display the title ID
    LongTitleName, ///< Display the long name of the title
    ListEnd,       ///< Keep this at the end of the enum.
};

struct Values {
    QByteArray geometry;
    QByteArray state;

    QByteArray renderwindow_geometry;

    QByteArray gamelist_header_state;

    QByteArray microprofile_geometry;
    bool microprofile_visible;

    bool single_window_mode;
    bool fullscreen;
    bool display_titlebar;
    bool show_filter_bar;
    bool show_status_bar;

    bool confirm_before_closing;
    bool first_start;
    bool pause_when_in_background;
    bool hide_mouse;

    bool updater_found;
    bool update_on_close;
    bool check_for_update_on_start;

    // Discord RPC
    bool enable_discord_presence;

    // Game List
    GameListIconSize game_list_icon_size;
    GameListText game_list_row_1;
    GameListText game_list_row_2;
    bool game_list_hide_no_icon;
    bool game_list_single_line_mode;

    u16 screenshot_resolution_factor;

    QString roms_path;
    QString symbols_path;
    QString movie_record_path;
    QString movie_playback_path;
    QString screenshot_path;
    QString video_dumping_path;
    QString game_dir_deprecated;
    bool game_dir_deprecated_deepscan;
    QVector<UISettings::GameDir> game_dirs;
    QStringList recent_files;
    QString language;

    QString theme;

    // Shortcut name <Shortcut, context>
    std::vector<Shortcut> shortcuts;

    uint32_t callout_flags;

    // multiplayer settings
    QString nickname;
    QString ip;
    QString port;
    QString room_nickname;
    QString room_name;
    quint32 max_player;
    QString room_port;
    uint host_type;
    qulonglong game_id;
    QString room_description;
    std::pair<std::vector<std::string>, std::vector<std::string>> ban_list;

    // logging
    bool show_console;
};

extern Values values;
} // namespace UISettings

Q_DECLARE_METATYPE(UISettings::GameDir*);
