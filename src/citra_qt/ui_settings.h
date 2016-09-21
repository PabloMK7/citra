// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <vector>
#include <QByteArray>
#include <QString>
#include <QStringList>

namespace UISettings {

using ContextualShortcut = std::pair<QString, int>;
using Shortcut = std::pair<QString, ContextualShortcut>;

struct Values {
    QByteArray geometry;
    QByteArray state;

    QByteArray renderwindow_geometry;

    QByteArray gamelist_header_state;

    QByteArray microprofile_geometry;
    bool microprofile_visible;

    bool single_window_mode;
    bool display_titlebar;

    bool confirm_before_closing;
    bool first_start;

    QString roms_path;
    QString symbols_path;
    QString gamedir;
    bool gamedir_deepscan;
    QStringList recent_files;

    // Shortcut name <Shortcut, context>
    std::vector<Shortcut> shortcuts;
};

extern Values values;
}
