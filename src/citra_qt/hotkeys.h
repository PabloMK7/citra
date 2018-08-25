// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <map>
#include "ui_hotkeys.h"

class QDialog;
class QKeySequence;
class QSettings;
class QShortcut;

class HotkeyRegistry final {
public:
    friend class GHotkeysDialog;

    explicit HotkeyRegistry();
    ~HotkeyRegistry();

    /**
     * Loads hotkeys from the settings file.
     *
     * @note Yet unregistered hotkeys which are present in the settings will automatically be
     *       registered.
     */
    void LoadHotkeys();

    /**
     * Saves all registered hotkeys to the settings file.
     *
     * @note Each hotkey group will be stored a settings group; For each hotkey inside that group, a
     *       settings group will be created to store the key sequence and the hotkey context.
     */
    void SaveHotkeys();

    /**
     * Returns a QShortcut object whose activated() signal can be connected to other QObjects'
     * slots.
     *
     * @param group  General group this hotkey belongs to (e.g. "Main Window", "Debugger").
     * @param action Name of the action (e.g. "Start Emulation", "Load Image").
     * @param widget Parent widget of the returned QShortcut.
     * @warning If multiple QWidgets' call this function for the same action, the returned QShortcut
     *          will be the same. Thus, you shouldn't rely on the caller really being the
     *          QShortcut's parent.
     */
    QShortcut* GetHotkey(const QString& group, const QString& action, QWidget* widget);

    /**
     * Register a hotkey.
     *
     * @param group General group this hotkey belongs to (e.g. "Main Window", "Debugger")
     * @param action Name of the action (e.g. "Start Emulation", "Load Image")
     * @param default_keyseq Default key sequence to assign if the hotkey wasn't present in the
     *                       settings file before
     * @param default_context Default context to assign if the hotkey wasn't present in the settings
     *                        file before
     * @warning Both the group and action strings will be displayed in the hotkey settings dialog
     */
    void RegisterHotkey(const QString& group, const QString& action,
                        const QKeySequence& default_keyseq = {},
                        Qt::ShortcutContext default_context = Qt::WindowShortcut);

private:
    struct Hotkey {
        QKeySequence keyseq;
        QShortcut* shortcut = nullptr;
        Qt::ShortcutContext context = Qt::WindowShortcut;
    };

    using HotkeyMap = std::map<QString, Hotkey>;
    using HotkeyGroupMap = std::map<QString, HotkeyMap>;

    HotkeyGroupMap hotkey_groups;
};

class GHotkeysDialog : public QWidget {
    Q_OBJECT

public:
    explicit GHotkeysDialog(QWidget* parent = nullptr);
    void retranslateUi();

    void Populate(const HotkeyRegistry& registry);

private:
    Ui::hotkeys ui;
};
