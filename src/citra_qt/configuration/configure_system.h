// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>
#include "common/common_types.h"

namespace Ui {
class ConfigureSystem;
}

namespace Service {
namespace CFG {
class Module;
} // namespace CFG
} // namespace Service

class ConfigureSystem : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureSystem(QWidget* parent = nullptr);
    ~ConfigureSystem() override;

    void applyConfiguration();
    void setConfiguration();
    void retranslateUi();

private:
    void ReadSystemSettings();
    void ConfigureTime();

    void UpdateBirthdayComboBox(int birthmonth_index);
    void UpdateInitTime(int init_clock);
    void RefreshConsoleID();

    std::unique_ptr<Ui::ConfigureSystem> ui;
    bool enabled = false;

    std::shared_ptr<Service::CFG::Module> cfg;
    std::u16string username;
    int birthmonth = 0;
    int birthday = 0;
    int language_index = 0;
    int sound_index = 0;
    u8 country_code;
    u16 play_coin;
};
