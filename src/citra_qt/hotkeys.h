// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "ui_hotkeys.h"

class QDialog;
class QKeySequence;
class QSettings;
class QShortcut;

/**
 * Register a hotkey.
 *
 * @param group General group this hotkey belongs to (e.g. "Main Window", "Debugger")
 * @param action Name of the action (e.g. "Start Emulation", "Load Image")
 * @param default_keyseq Default key sequence to assign if the hotkey wasn't present in the settings file before
 * @param default_context Default context to assign if the hotkey wasn't present in the settings file before
 * @warning Both the group and action strings will be displayed in the hotkey settings dialog
 */
void RegisterHotkey(const QString& group, const QString& action, const QKeySequence& default_keyseq = QKeySequence(), Qt::ShortcutContext default_context = Qt::WindowShortcut);

/**
 * Returns a QShortcut object whose activated() signal can be connected to other QObjects' slots.
 *
 * @param widget Parent widget of the returned QShortcut.
 * @warning If multiple QWidgets' call this function for the same action, the returned QShortcut will be the same. Thus, you shouldn't rely on the caller really being the QShortcut's parent.
 */
QShortcut* GetHotkey(const QString& group, const QString& action, QWidget* widget);

/**
 * Saves all registered hotkeys to the settings file.
 *
 * @note Each hotkey group will be stored a settings group; For each hotkey inside that group, a settings group will be created to store the key sequence and the hotkey context.
 */
void SaveHotkeys(QSettings& settings);

/**
 * Loads hotkeys from the settings file.
 *
 * @note Yet unregistered hotkeys which are present in the settings will automatically be registered.
 */
void LoadHotkeys(QSettings& settings);

class GHotkeysDialog : public QDialog
{
    Q_OBJECT

public:
    GHotkeysDialog(QWidget* parent = NULL);

private:
    Ui::hotkeys ui;
};
