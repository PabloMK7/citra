// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDirIterator>
#include "citra_qt/configuration/configure_ui.h"
#include "citra_qt/ui_settings.h"
#include "ui_configure_ui.h"

ConfigureUi::ConfigureUi(QWidget* parent) : QWidget(parent), ui(new Ui::ConfigureUi) {
    ui->setupUi(this);
    ui->language_combobox->addItem(tr("<System>"), QString(""));
    ui->language_combobox->addItem(tr("English"), QString("en"));
    QDirIterator it(":/languages", QDirIterator::NoIteratorFlags);
    while (it.hasNext()) {
        QString locale = it.next();
        locale.truncate(locale.lastIndexOf('.'));
        locale.remove(0, locale.lastIndexOf('/') + 1);
        QString lang = QLocale::languageToString(QLocale(locale).language());
        ui->language_combobox->addItem(lang, locale);
    }

    // Unlike other configuration changes, interface language changes need to be reflected on the
    // interface immediately. This is done by passing a signal to the main window, and then
    // retranslating when passing back.
    connect(ui->language_combobox,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &ConfigureUi::onLanguageChanged);

    for (const auto& theme : UISettings::themes) {
        ui->theme_combobox->addItem(theme.first, theme.second);
    }

    this->setConfiguration();
}

ConfigureUi::~ConfigureUi() = default;

void ConfigureUi::setConfiguration() {
    ui->theme_combobox->setCurrentIndex(ui->theme_combobox->findData(UISettings::values.theme));
    ui->language_combobox->setCurrentIndex(
        ui->language_combobox->findData(UISettings::values.language));
    ui->icon_size_combobox->setCurrentIndex(
        static_cast<int>(UISettings::values.game_list_icon_size));
    ui->row_1_text_combobox->setCurrentIndex(static_cast<int>(UISettings::values.game_list_row_1));
    ui->row_2_text_combobox->setCurrentIndex(static_cast<int>(UISettings::values.game_list_row_2) +
                                             1);
    ui->toggle_hide_no_icon->setChecked(UISettings::values.game_list_hide_no_icon);
}

void ConfigureUi::applyConfiguration() {
    UISettings::values.theme =
        ui->theme_combobox->itemData(ui->theme_combobox->currentIndex()).toString();
    UISettings::values.game_list_icon_size =
        static_cast<UISettings::GameListIconSize>(ui->icon_size_combobox->currentIndex());
    UISettings::values.game_list_row_1 =
        static_cast<UISettings::GameListText>(ui->row_1_text_combobox->currentIndex());
    UISettings::values.game_list_row_2 =
        static_cast<UISettings::GameListText>(ui->row_2_text_combobox->currentIndex() - 1);
    UISettings::values.game_list_hide_no_icon = ui->toggle_hide_no_icon->isChecked();
}

void ConfigureUi::onLanguageChanged(int index) {
    if (index == -1)
        return;

    emit languageChanged(ui->language_combobox->itemData(index).toString());
}

void ConfigureUi::retranslateUi() {
    ui->retranslateUi(this);
}
