// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/config.h"
#include "citra_qt/configure_dialog.h"
#include "ui_configure.h"


#include "core/settings.h"

ConfigureDialog::ConfigureDialog(QWidget *parent, bool running) :
    QDialog(parent),
    ui(new Ui::ConfigureDialog),
    emulation_running(running)
{
    ui->setupUi(this);
    this->setConfiguration();
}

ConfigureDialog::~ConfigureDialog() {
}

void ConfigureDialog::setConfiguration() {
    // System tab needs set manually
    // depending on whether emulation is running
    ui->systemTab->setConfiguration(emulation_running);
}

void ConfigureDialog::applyConfiguration() {
    ui->generalTab->applyConfiguration();
    ui->systemTab->applyConfiguration();
    ui->inputTab->applyConfiguration();
    ui->graphicsTab->applyConfiguration();
    ui->audioTab->applyConfiguration();
    ui->debugTab->applyConfiguration();
}
