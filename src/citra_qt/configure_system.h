// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class ConfigureSystem;
}

class ConfigureSystem : public QWidget
{
    Q_OBJECT

public:
    explicit ConfigureSystem(QWidget *parent = nullptr);
    ~ConfigureSystem();

    void applyConfiguration();
    void setConfiguration(bool emulation_running);

public slots:
    void updateBirthdayComboBox(int birthmonth_index);

private:
    void ReadSystemSettings();

    std::unique_ptr<Ui::ConfigureSystem> ui;
    bool enabled;

    std::u16string username;
    int birthmonth, birthday;
    int language_index;
    int sound_index;
};
