// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/configure_graphics.h"
#include "ui_configure_graphics.h"

#include "core/settings.h"
#include "core/system.h"

ConfigureGraphics::ConfigureGraphics(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConfigureGraphics)
{
    ui->setupUi(this);
    this->setConfiguration();

    ui->toggle_vsync->setEnabled(!System::IsPoweredOn());
}

ConfigureGraphics::~ConfigureGraphics() {
}

void ConfigureGraphics::setConfiguration() {
    ui->toggle_hw_renderer->setChecked(Settings::values.use_hw_renderer);
    ui->toggle_shader_jit->setChecked(Settings::values.use_shader_jit);
    ui->toggle_scaled_resolution->setChecked(Settings::values.use_scaled_resolution);
    ui->toggle_vsync->setChecked(Settings::values.use_vsync);
}

void ConfigureGraphics::applyConfiguration() {
    Settings::values.use_hw_renderer = ui->toggle_hw_renderer->isChecked();
    Settings::values.use_shader_jit = ui->toggle_shader_jit->isChecked();
    Settings::values.use_scaled_resolution = ui->toggle_scaled_resolution->isChecked();
    Settings::values.use_vsync = ui->toggle_vsync->isChecked();
    Settings::Apply();
}
