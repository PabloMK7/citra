// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/configure_system.h"
#include "citra_qt/ui_settings.h"
#include "ui_configure_system.h"

#include "core/hle/service/fs/archive.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/system.h"

static const std::array<int, 12> days_in_month = {{
    31, 29, 31, 30, 31, 30, 31, 31, 30, 31, 30, 31
}};

ConfigureSystem::ConfigureSystem(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConfigureSystem) {
    ui->setupUi(this);
    connect(ui->combo_birthmonth, SIGNAL(currentIndexChanged(int)), SLOT(updateBirthdayComboBox(int)));

    this->setConfiguration();
}

ConfigureSystem::~ConfigureSystem() {
}

void ConfigureSystem::setConfiguration() {
    enabled = !System::IsPoweredOn();

    if (!enabled) {
        ReadSystemSettings();
        ui->group_system_settings->setEnabled(false);
    } else {
        // This tab is enabled only when game is not running (i.e. all service are not initialized).
        // Temporarily register archive types and load the config savegame file to memory.
        Service::FS::RegisterArchiveTypes();
        ResultCode result = Service::CFG::LoadConfigNANDSaveFile();
        Service::FS::UnregisterArchiveTypes();

        if (result.IsError()) {
            ui->label_disable_info->setText(tr("Failed to load system settings data."));
            ui->group_system_settings->setEnabled(false);
            enabled = false;
            return;
        }

        ReadSystemSettings();
        ui->label_disable_info->hide();
    }
}

void ConfigureSystem::ReadSystemSettings() {
    // set username
    username = Service::CFG::GetUsername();
    // ui->edit_username->setText(QString::fromStdU16String(username)); // TODO(wwylele): Use this when we move to Qt 5.5
    ui->edit_username->setText(QString::fromUtf16(reinterpret_cast<const ushort*>(username.data())));

    // set birthday
    std::tie(birthmonth, birthday) = Service::CFG::GetBirthday();
    ui->combo_birthmonth->setCurrentIndex(birthmonth - 1);
    updateBirthdayComboBox(birthmonth - 1); // explicitly update it because the signal from setCurrentIndex is not reliable
    ui->combo_birthday->setCurrentIndex(birthday - 1);

    // set system language
    language_index = Service::CFG::GetSystemLanguage();
    ui->combo_language->setCurrentIndex(language_index);

    // set sound output mode
    sound_index = Service::CFG::GetSoundOutputMode();
    ui->combo_sound->setCurrentIndex(sound_index);
}

void ConfigureSystem::applyConfiguration() {
    if (!enabled)
        return;

    bool modified = false;

    // apply username
    // std::u16string new_username = ui->edit_username->text().toStdU16String(); // TODO(wwylele): Use this when we move to Qt 5.5
    std::u16string new_username(reinterpret_cast<const char16_t*>(ui->edit_username->text().utf16()));
    if (new_username != username) {
        Service::CFG::SetUsername(new_username);
        modified = true;
    }

    // apply birthday
    int new_birthmonth = ui->combo_birthmonth->currentIndex() + 1;
    int new_birthday = ui->combo_birthday->currentIndex() + 1;
    if (birthmonth != new_birthmonth || birthday != new_birthday) {
        Service::CFG::SetBirthday(new_birthmonth, new_birthday);
        modified = true;
    }

    // apply language
    int new_language = ui->combo_language->currentIndex();
    if (language_index != new_language) {
        Service::CFG::SetSystemLanguage(static_cast<Service::CFG::SystemLanguage>(new_language));
        modified = true;
    }

    // apply sound
    int new_sound = ui->combo_sound->currentIndex();
    if (sound_index != new_sound) {
        Service::CFG::SetSoundOutputMode(static_cast<Service::CFG::SoundOutputMode>(new_sound));
        modified = true;
    }

    // update the config savegame if any item is modified.
    if (modified)
        Service::CFG::UpdateConfigNANDSavegame();
}

void ConfigureSystem::updateBirthdayComboBox(int birthmonth_index) {
    if (birthmonth_index < 0 || birthmonth_index >= 12)
        return;

    // store current day selection
    int birthday_index = ui->combo_birthday->currentIndex();

    // get number of days in the new selected month
    int days = days_in_month[birthmonth_index];

    // if the selected day is out of range,
    // reset it to 1st
    if (birthday_index < 0 || birthday_index >= days)
        birthday_index = 0;

    // update the day combo box
    ui->combo_birthday->clear();
    for (int i = 1; i <= days; ++i) {
        ui->combo_birthday->addItem(QString::number(i));
    }

    // restore the day selection
    ui->combo_birthday->setCurrentIndex(birthday_index);
}
