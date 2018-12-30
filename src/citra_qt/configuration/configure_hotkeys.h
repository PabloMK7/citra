// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>
#include "core/settings.h"

namespace Ui {
class ConfigureHotkeys;
}

class HotkeyRegistry;
class QStandardItemModel;

class ConfigureHotkeys : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureHotkeys(QWidget* parent = nullptr);
    ~ConfigureHotkeys();

    void applyConfiguration(HotkeyRegistry& registry);
    void retranslateUi();

    void EmitHotkeysChanged();

    /**
     * Populates the hotkey list widget using data from the provided registry.
     * Called everytime the Configure dialog is opened.
     * @param registry The HotkeyRegistry whose data is used to populate the list.
     */
    void Populate(const HotkeyRegistry& registry);

public slots:
    void OnInputKeysChanged(QList<QKeySequence> new_key_list);

signals:
    void HotkeysChanged(QList<QKeySequence> new_key_list);

private:
    void Configure(QModelIndex index);
    bool IsUsedKey(QKeySequence key_sequence);
    QList<QKeySequence> GetUsedKeyList();

    std::unique_ptr<Ui::ConfigureHotkeys> ui;

    /**
     * List of keyboard keys currently registered to any of the 3DS inputs.
     * These can't be bound to any hotkey.
     * Synchronised with ConfigureInput via signal-slot.
     */
    QList<QKeySequence> input_keys_list;

    QStandardItemModel* model;
};
