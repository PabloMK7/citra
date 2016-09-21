// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/configure_general.h"
#include "citra_qt/ui_settings.h"
#include "core/settings.h"
#include "core/system.h"
#include "ui_configure_general.h"

ConfigureGeneral::ConfigureGeneral(QWidget* parent)
    : QWidget(parent), ui(new Ui::ConfigureGeneral) {

    ui->setupUi(this);
    this->setConfiguration();

    ui->toggle_cpu_jit->setEnabled(!System::IsPoweredOn());
}

ConfigureGeneral::~ConfigureGeneral() {}

void ConfigureGeneral::setConfiguration() {
    ui->toggle_deepscan->setChecked(UISettings::values.gamedir_deepscan);
    ui->toggle_check_exit->setChecked(UISettings::values.confirm_before_closing);
    ui->toggle_cpu_jit->setChecked(Settings::values.use_cpu_jit);
    ui->region_combobox->setCurrentIndex(Settings::values.region_value);
}

void ConfigureGeneral::applyConfiguration() {
    UISettings::values.gamedir_deepscan = ui->toggle_deepscan->isChecked();
    UISettings::values.confirm_before_closing = ui->toggle_check_exit->isChecked();
    Settings::values.region_value = ui->region_combobox->currentIndex();
    Settings::values.use_cpu_jit = ui->toggle_cpu_jit->isChecked();
    Settings::Apply();
}
