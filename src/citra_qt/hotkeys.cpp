// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>

#include <QKeySequence>
#include <QSettings>
#include <QShortcut>

#include "hotkeys.h"

struct Hotkey
{
    Hotkey() : shortcut(nullptr), context(Qt::WindowShortcut) {}

    QKeySequence keyseq;
    QShortcut* shortcut;
    Qt::ShortcutContext context;
};

typedef std::map<QString, Hotkey> HotkeyMap;
typedef std::map<QString, HotkeyMap> HotkeyGroupMap;

HotkeyGroupMap hotkey_groups;

void SaveHotkeys(QSettings& settings)
{
    settings.beginGroup("Shortcuts");

    for (auto group : hotkey_groups)
    {
        settings.beginGroup(group.first);
        for (auto hotkey : group.second)
        {
            settings.beginGroup(hotkey.first);
            settings.setValue(QString("KeySeq"), hotkey.second.keyseq.toString());
            settings.setValue(QString("Context"), hotkey.second.context);
            settings.endGroup();
        }
        settings.endGroup();
    }
    settings.endGroup();
}

void LoadHotkeys(QSettings& settings)
{
    settings.beginGroup("Shortcuts");

    // Make sure NOT to use a reference here because it would become invalid once we call beginGroup()
    QStringList groups = settings.childGroups();
    for (auto group : groups)
    {
        settings.beginGroup(group);

        QStringList hotkeys = settings.childGroups();
        for (auto hotkey : hotkeys)
        {
            settings.beginGroup(hotkey);

            // RegisterHotkey assigns default keybindings, so use old values as default parameters
            Hotkey& hk = hotkey_groups[group][hotkey];
            hk.keyseq = QKeySequence::fromString(settings.value("KeySeq", hk.keyseq.toString()).toString());
            hk.context = (Qt::ShortcutContext)settings.value("Context", hk.context).toInt();
            if (hk.shortcut)
                hk.shortcut->setKey(hk.keyseq);

            settings.endGroup();
        }

        settings.endGroup();
    }

    settings.endGroup();
}

void RegisterHotkey(const QString& group, const QString& action, const QKeySequence& default_keyseq, Qt::ShortcutContext default_context)
{
    if (hotkey_groups[group].find(action) == hotkey_groups[group].end())
    {
        hotkey_groups[group][action].keyseq = default_keyseq;
        hotkey_groups[group][action].context = default_context;
    }
}

QShortcut* GetHotkey(const QString& group, const QString& action, QWidget* widget)
{
    Hotkey& hk = hotkey_groups[group][action];

    if (!hk.shortcut)
        hk.shortcut = new QShortcut(hk.keyseq, widget, nullptr, nullptr, hk.context);

    return hk.shortcut;
}


GHotkeysDialog::GHotkeysDialog(QWidget* parent): QDialog(parent)
{
    ui.setupUi(this);

    for (auto group : hotkey_groups)
    {
        QTreeWidgetItem* toplevel_item = new QTreeWidgetItem(QStringList(group.first));
        for (auto hotkey : group.second)
        {
            QStringList columns;
            columns << hotkey.first << hotkey.second.keyseq.toString();
            QTreeWidgetItem* item = new QTreeWidgetItem(columns);
            toplevel_item->addChild(item);
        }
        ui.treeWidget->addTopLevelItem(toplevel_item);
    }
    // TODO: Make context configurable as well (hiding the column for now)
    ui.treeWidget->setColumnCount(2);

    ui.treeWidget->resizeColumnToContents(0);
    ui.treeWidget->resizeColumnToContents(1);
}
