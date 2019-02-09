// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include <QDialog>
#include "core/frontend/applets/mii_selector.h"

class QDialogButtonBox;
class QComboBox;
class QVBoxLayout;
class QtMiiSelector;

class QtMiiSelectorDialog final : public QDialog {
    Q_OBJECT

public:
    QtMiiSelectorDialog(QWidget* parent, QtMiiSelector* mii_selector_);

private:
    void ShowNoMiis();

    QDialogButtonBox* buttons;
    QComboBox* combobox;
    QVBoxLayout* layout;
    QtMiiSelector* mii_selector;
    u32 return_code = 0;
    std::unordered_map<int, HLE::Applets::MiiData> miis;

    friend class QtMiiSelector;
};

class QtMiiSelector final : public QObject, public Frontend::MiiSelector {
    Q_OBJECT

public:
    explicit QtMiiSelector(QWidget& parent);
    void Setup(const Frontend::MiiSelectorConfig* config) override;

private:
    Q_INVOKABLE void OpenDialog();

    QWidget& parent;

    friend class QtMiiSelectorDialog;
};
