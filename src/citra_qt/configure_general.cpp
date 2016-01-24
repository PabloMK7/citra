// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/configure_general.h"
#include "citra_qt/ui_configure_general.h"
#include "citra_qt/ui_settings.h"

#include "core/settings.h"

#include "video_core/video_core.h"

ConfigureGeneral::ConfigureGeneral(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConfigureGeneral)
{
    ui->setupUi(this);
    this->setConfiguration();
}

ConfigureGeneral::~ConfigureGeneral()
{
    delete ui;
}

void ConfigureGeneral::setConfiguration() {
    ui->toogleCheckExit->setChecked(UISettings::values.check_closure);
    ui->toogleHWRenderer->setChecked(Settings::values.use_hw_renderer);
    ui->toogleShaderJIT->setChecked(Settings::values.use_shader_jit);
}

void ConfigureGeneral::applyConfiguration() {
    UISettings::values.check_closure = ui->toogleCheckExit->isChecked();

    VideoCore::g_hw_renderer_enabled =
    Settings::values.use_hw_renderer = ui->toogleHWRenderer->isChecked();

    VideoCore::g_shader_jit_enabled =
    Settings::values.use_shader_jit = ui->toogleShaderJIT->isChecked();
}
