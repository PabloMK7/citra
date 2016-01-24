// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifndef UISETTINGS_H
#define UISETTINGS_H

#include <QByteArray>
#include <QStringList>
#include <QString>

#include <vector>

namespace UISettings {

    typedef std::pair<QString, int> ContextedShortcut;
    typedef std::pair<QString, ContextedShortcut> Shortcut;

struct Values {
    QByteArray geometry;
    QByteArray state;

    QByteArray renderwindow_geometry;

    QByteArray gamelist_header_state;

    QByteArray microprofile_geometry;
    bool microprofile_visible;

    bool single_window_mode;
    bool display_titlebar;

    bool check_closure;
    bool first_start;

    QString roms_path;
    QString symbols_path;
    QString gamedir_path;
    bool gamedir_deepscan;
    QStringList recent_files;

    // Shortcut name <Shortcut, context>
    std::vector<Shortcut> shortcuts;
} extern values;

}

#endif // UISETTINGS_H
