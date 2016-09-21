// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <map>
#include <QKeySequence>
#include <QShortcut>
#include <QtGlobal>
#include "citra_qt/hotkeys.h"
#include "citra_qt/ui_settings.h"

struct Hotkey {
    Hotkey() : shortcut(nullptr), context(Qt::WindowShortcut) {}

    QKeySequence keyseq;
    QShortcut* shortcut;
    Qt::ShortcutContext context;
};

typedef std::map<QString, Hotkey> HotkeyMap;
typedef std::map<QString, HotkeyMap> HotkeyGroupMap;

HotkeyGroupMap hotkey_groups;

void SaveHotkeys() {
    UISettings::values.shortcuts.clear();
    for (auto group : hotkey_groups) {
        for (auto hotkey : group.second) {
            UISettings::values.shortcuts.emplace_back(
                UISettings::Shortcut(group.first + "/" + hotkey.first,
                                     UISettings::ContextualShortcut(hotkey.second.keyseq.toString(),
                                                                    hotkey.second.context)));
        }
    }
}

void LoadHotkeys() {
    // Make sure NOT to use a reference here because it would become invalid once we call
    // beginGroup()
    for (auto shortcut : UISettings::values.shortcuts) {
        QStringList cat = shortcut.first.split("/");
        Q_ASSERT(cat.size() >= 2);

        // RegisterHotkey assigns default keybindings, so use old values as default parameters
        Hotkey& hk = hotkey_groups[cat[0]][cat[1]];
        if (!shortcut.second.first.isEmpty()) {
            hk.keyseq = QKeySequence::fromString(shortcut.second.first);
            hk.context = (Qt::ShortcutContext)shortcut.second.second;
        }
        if (hk.shortcut)
            hk.shortcut->setKey(hk.keyseq);
    }
}

void RegisterHotkey(const QString& group, const QString& action, const QKeySequence& default_keyseq,
                    Qt::ShortcutContext default_context) {
    if (hotkey_groups[group].find(action) == hotkey_groups[group].end()) {
        hotkey_groups[group][action].keyseq = default_keyseq;
        hotkey_groups[group][action].context = default_context;
    }
}

QShortcut* GetHotkey(const QString& group, const QString& action, QWidget* widget) {
    Hotkey& hk = hotkey_groups[group][action];

    if (!hk.shortcut)
        hk.shortcut = new QShortcut(hk.keyseq, widget, nullptr, nullptr, hk.context);

    return hk.shortcut;
}

GHotkeysDialog::GHotkeysDialog(QWidget* parent) : QWidget(parent) {
    ui.setupUi(this);

    for (auto group : hotkey_groups) {
        QTreeWidgetItem* toplevel_item = new QTreeWidgetItem(QStringList(group.first));
        for (auto hotkey : group.second) {
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
