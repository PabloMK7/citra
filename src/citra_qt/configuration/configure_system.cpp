// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QMessageBox>
#include "citra_qt/configuration/configure_system.h"
#include "citra_qt/ui_settings.h"
#include "core/core.h"
#include "core/hle/service/cfg/cfg.h"
#include "core/hle/service/fs/archive.h"
#include "core/settings.h"
#include "ui_configure_system.h"

static const std::array<int, 12> days_in_month = {{
    31,
    29,
    31,
    30,
    31,
    30,
    31,
    31,
    30,
    31,
    30,
    31,
}};

static const std::array<const char*, 187> country_names = {
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "Japan"),
    "",
    "",
    "",
    "",
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "Anguilla"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Antigua and Barbuda"), // 0-9
    QT_TRANSLATE_NOOP("ConfigureSystem", "Argentina"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Aruba"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Bahamas"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Barbados"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Belize"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Bolivia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Brazil"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "British Virgin Islands"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Canada"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Cayman Islands"), // 10-19
    QT_TRANSLATE_NOOP("ConfigureSystem", "Chile"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Colombia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Costa Rica"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Dominica"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Dominican Republic"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Ecuador"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "El Salvador"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "French Guiana"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Grenada"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Guadeloupe"), // 20-29
    QT_TRANSLATE_NOOP("ConfigureSystem", "Guatemala"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Guyana"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Haiti"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Honduras"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Jamaica"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Martinique"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Mexico"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Montserrat"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Netherlands Antilles"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Nicaragua"), // 30-39
    QT_TRANSLATE_NOOP("ConfigureSystem", "Panama"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Paraguay"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Peru"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Saint Kitts and Nevis"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Saint Lucia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Saint Vincent and the Grenadines"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Suriname"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Trinidad and Tobago"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Turks and Caicos Islands"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "United States"), // 40-49
    QT_TRANSLATE_NOOP("ConfigureSystem", "Uruguay"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "US Virgin Islands"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Venezuela"),
    "",
    "",
    "",
    "",
    "",
    "",
    "", // 50-59
    "",
    "",
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "Albania"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Australia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Austria"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Belgium"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Bosnia and Herzegovina"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Botswana"), // 60-69
    QT_TRANSLATE_NOOP("ConfigureSystem", "Bulgaria"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Croatia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Cyprus"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Czech Republic"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Denmark"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Estonia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Finland"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "France"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Germany"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Greece"), // 70-79
    QT_TRANSLATE_NOOP("ConfigureSystem", "Hungary"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Iceland"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Ireland"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Italy"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Latvia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Lesotho"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Liechtenstein"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Lithuania"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Luxembourg"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Macedonia"), // 80-89
    QT_TRANSLATE_NOOP("ConfigureSystem", "Malta"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Montenegro"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Mozambique"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Namibia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Netherlands"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "New Zealand"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Norway"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Poland"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Portugal"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Romania"), // 90-99
    QT_TRANSLATE_NOOP("ConfigureSystem", "Russia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Serbia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Slovakia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Slovenia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "South Africa"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Spain"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Swaziland"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Sweden"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Switzerland"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Turkey"), // 100-109
    QT_TRANSLATE_NOOP("ConfigureSystem", "United Kingdom"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Zambia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Zimbabwe"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Azerbaijan"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Mauritania"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Mali"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Niger"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Chad"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Sudan"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Eritrea"), // 110-119
    QT_TRANSLATE_NOOP("ConfigureSystem", "Djibouti"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Somalia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Andorra"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Gibraltar"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Guernsey"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Isle of Man"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Jersey"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Monaco"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Taiwan"),
    "", // 120-129
    "",
    "",
    "",
    "",
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "South Korea"),
    "",
    "",
    "", // 130-139
    "",
    "",
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "Hong Kong"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Macau"),
    "",
    "",
    "",
    "", // 140-149
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "Indonesia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Singapore"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Thailand"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Philippines"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Malaysia"),
    "",
    "",
    "", // 150-159
    QT_TRANSLATE_NOOP("ConfigureSystem", "China"),
    "",
    "",
    "",
    "",
    "",
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "United Arab Emirates"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "India"), // 160-169
    QT_TRANSLATE_NOOP("ConfigureSystem", "Egypt"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Oman"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Qatar"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Kuwait"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Saudi Arabia"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Syria"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Bahrain"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Jordan"),
    "",
    "", // 170-179
    "",
    "",
    "",
    "",
    QT_TRANSLATE_NOOP("ConfigureSystem", "San Marino"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Vatican City"),
    QT_TRANSLATE_NOOP("ConfigureSystem", "Bermuda"), // 180-186
};

ConfigureSystem::ConfigureSystem(QWidget* parent) : QWidget(parent), ui(new Ui::ConfigureSystem) {
    ui->setupUi(this);
    connect(ui->combo_birthmonth,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &ConfigureSystem::updateBirthdayComboBox);
    connect(ui->combo_init_clock,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &ConfigureSystem::updateInitTime);
    connect(ui->button_regenerate_console_id, &QPushButton::clicked, this,
            &ConfigureSystem::refreshConsoleID);
    for (u8 i = 0; i < country_names.size(); i++) {
        if (country_names.at(i) != "") {
            ui->combo_country->addItem(tr(country_names.at(i)), i);
        }
    }

    ConfigureTime();
}

ConfigureSystem::~ConfigureSystem() = default;

void ConfigureSystem::setConfiguration() {
    enabled = !Core::System::GetInstance().IsPoweredOn();

    ui->combo_init_clock->setCurrentIndex(static_cast<u8>(Settings::values.init_clock));
    QDateTime date_time;
    date_time.setTime_t(Settings::values.init_time);
    ui->edit_init_time->setDateTime(date_time);

    if (!enabled) {
        cfg = Service::CFG::GetCurrentModule();
        ReadSystemSettings();
        ui->group_system_settings->setEnabled(false);
    } else {
        // This tab is enabled only when game is not running (i.e. all service are not initialized).
        // Temporarily register archive types and load the config savegame file to memory.
        Service::FS::RegisterArchiveTypes();
        cfg = std::make_shared<Service::CFG::Module>();
        Service::FS::UnregisterArchiveTypes();

        ReadSystemSettings();
        ui->label_disable_info->hide();
    }
}

void ConfigureSystem::ReadSystemSettings() {
    // set username
    username = cfg->GetUsername();
    // TODO(wwylele): Use this when we move to Qt 5.5
    // ui->edit_username->setText(QString::fromStdU16String(username));
    ui->edit_username->setText(
        QString::fromUtf16(reinterpret_cast<const ushort*>(username.data())));

    // set birthday
    std::tie(birthmonth, birthday) = cfg->GetBirthday();
    ui->combo_birthmonth->setCurrentIndex(birthmonth - 1);
    updateBirthdayComboBox(
        birthmonth -
        1); // explicitly update it because the signal from setCurrentIndex is not reliable
    ui->combo_birthday->setCurrentIndex(birthday - 1);

    // set system language
    language_index = cfg->GetSystemLanguage();
    ui->combo_language->setCurrentIndex(language_index);

    // set sound output mode
    sound_index = cfg->GetSoundOutputMode();
    ui->combo_sound->setCurrentIndex(sound_index);

    // set the country code
    country_code = cfg->GetCountryCode();
    ui->combo_country->setCurrentIndex(ui->combo_country->findData(country_code));

    // set the console id
    u64 console_id = cfg->GetConsoleUniqueId();
    ui->label_console_id->setText(
        tr("Console ID: 0x%1").arg(QString::number(console_id, 16).toUpper()));
}

void ConfigureSystem::applyConfiguration() {
    if (!enabled)
        return;

    bool modified = false;

    // apply username
    // TODO(wwylele): Use this when we move to Qt 5.5
    // std::u16string new_username = ui->edit_username->text().toStdU16String();
    std::u16string new_username(
        reinterpret_cast<const char16_t*>(ui->edit_username->text().utf16()));
    if (new_username != username) {
        cfg->SetUsername(new_username);
        modified = true;
    }

    // apply birthday
    int new_birthmonth = ui->combo_birthmonth->currentIndex() + 1;
    int new_birthday = ui->combo_birthday->currentIndex() + 1;
    if (birthmonth != new_birthmonth || birthday != new_birthday) {
        cfg->SetBirthday(new_birthmonth, new_birthday);
        modified = true;
    }

    // apply language
    int new_language = ui->combo_language->currentIndex();
    if (language_index != new_language) {
        cfg->SetSystemLanguage(static_cast<Service::CFG::SystemLanguage>(new_language));
        modified = true;
    }

    // apply sound
    int new_sound = ui->combo_sound->currentIndex();
    if (sound_index != new_sound) {
        cfg->SetSoundOutputMode(static_cast<Service::CFG::SoundOutputMode>(new_sound));
        modified = true;
    }

    // apply country
    u8 new_country = static_cast<u8>(ui->combo_country->currentData().toInt());
    if (country_code != new_country) {
        cfg->SetCountryCode(new_country);
        modified = true;
    }

    // update the config savegame if any item is modified.
    if (modified)
        cfg->UpdateConfigNANDSavegame();

    Settings::values.init_clock =
        static_cast<Settings::InitClock>(ui->combo_init_clock->currentIndex());
    Settings::values.init_time = ui->edit_init_time->dateTime().toTime_t();
    Settings::Apply();
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

void ConfigureSystem::ConfigureTime() {
    ui->edit_init_time->setCalendarPopup(true);
    QDateTime dt;
    dt.fromString("2000-01-01 00:00:01", "yyyy-MM-dd hh:mm:ss");
    ui->edit_init_time->setMinimumDateTime(dt);

    this->setConfiguration();

    updateInitTime(ui->combo_init_clock->currentIndex());
}

void ConfigureSystem::updateInitTime(int init_clock) {
    const bool is_fixed_time =
        static_cast<Settings::InitClock>(init_clock) == Settings::InitClock::FixedTime;
    ui->label_init_time->setVisible(is_fixed_time);
    ui->edit_init_time->setVisible(is_fixed_time);
}

void ConfigureSystem::refreshConsoleID() {
    QMessageBox::StandardButton reply;
    QString warning_text = tr("This will replace your current virtual 3DS with a new one. "
                              "Your current virtual 3DS will not be recoverable. "
                              "This might have unexpected effects in games. This might fail, "
                              "if you use an outdated config savegame. Continue?");
    reply = QMessageBox::critical(this, tr("Warning"), warning_text,
                                  QMessageBox::No | QMessageBox::Yes);
    if (reply == QMessageBox::No)
        return;
    u32 random_number;
    u64 console_id;
    cfg->GenerateConsoleUniqueId(random_number, console_id);
    cfg->SetConsoleUniqueId(random_number, console_id);
    cfg->UpdateConfigNANDSavegame();
    ui->label_console_id->setText(
        tr("Console ID: 0x%1").arg(QString::number(console_id, 16).toUpper()));
}

void ConfigureSystem::retranslateUi() {
    ui->retranslateUi(this);
    ReadSystemSettings();
}
