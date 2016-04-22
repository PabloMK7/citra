// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/configure_general.h"
#include "citra_qt/ui_settings.h"
#include "ui_configure_general.h"

#include "core/settings.h"

ConfigureGeneral::ConfigureGeneral(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConfigureGeneral)
{
    ui->setupUi(this);
    this->setConfiguration();
}

ConfigureGeneral::~ConfigureGeneral() {
}

void ConfigureGeneral::setConfiguration() {
    ui->toogle_deepscan->setChecked(UISettings::values.gamedir_deepscan);
    ui->toogle_check_exit->setChecked(UISettings::values.confirm_before_closing);
    ui->region_combobox->setCurrentIndex(Settings::values.region_value);
    ui->toogle_hw_renderer->setChecked(Settings::values.use_hw_renderer);
    ui->toogle_shader_jit->setChecked(Settings::values.use_shader_jit);
    ui->toogle_scaled_resolution->setChecked(Settings::values.use_scaled_resolution);
}

void ConfigureGeneral::applyConfiguration() {
    UISettings::values.gamedir_deepscan = ui->toogle_deepscan->isChecked();
    UISettings::values.confirm_before_closing = ui->toogle_check_exit->isChecked();
    Settings::values.region_value = ui->region_combobox->currentIndex();
    Settings::values.use_hw_renderer = ui->toogle_hw_renderer->isChecked();
    Settings::values.use_shader_jit = ui->toogle_shader_jit->isChecked();
    Settings::values.use_scaled_resolution = ui->toogle_scaled_resolution->isChecked();
    Settings::Apply();
}
