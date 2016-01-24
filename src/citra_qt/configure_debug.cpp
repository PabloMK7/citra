// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/core.h"
#include "core/gdbstub/gdbstub.h" // TODO: can't include gdbstub without core.h
#include "core/settings.h"

#include "configure_debug.h"
#include "ui_configure_debug.h"

ConfigureDebug::ConfigureDebug(QWidget *parent) :
    QWidget(parent),
    ui(new Ui::ConfigureDebug)
{
    ui->setupUi(this);
    this->setConfiguration();
}

ConfigureDebug::~ConfigureDebug() {
    delete ui;
}

void ConfigureDebug::setConfiguration() {
    ui->toogleGDBStub->setChecked(Settings::values.use_gdbstub);
    ui->GDBPortSpinBox->setValue(Settings::values.gdbstub_port);
}

void ConfigureDebug::applyConfiguration() {
    GDBStub::ToggleServer(ui->toogleGDBStub->isChecked());
    Settings::values.use_gdbstub = ui->toogleGDBStub->isChecked();
    Settings::values.gdbstub_port = ui->GDBPortSpinBox->value();
}
