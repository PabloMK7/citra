// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/configure_debug.h"
#include "ui_configure_debug.h"

#include "core/settings.h"

ConfigureDebug::ConfigureDebug(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConfigureDebug)
{
    ui->setupUi(this);
    this->setConfiguration();
}

ConfigureDebug::~ConfigureDebug() {
}

void ConfigureDebug::setConfiguration() {
    ui->toggle_gdbstub->setChecked(Settings::values.use_gdbstub);
    ui->gdbport_spinbox->setEnabled(Settings::values.use_gdbstub);
    ui->gdbport_spinbox->setValue(Settings::values.gdbstub_port);
}

void ConfigureDebug::applyConfiguration() {
    Settings::values.use_gdbstub = ui->toggle_gdbstub->isChecked();
    Settings::values.gdbstub_port = ui->gdbport_spinbox->value();
    Settings::Apply();
}
