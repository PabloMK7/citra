// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "configure_dialog.h"
#include "ui_configure.h"

#include "config.h"

#include "core/settings.h"

ConfigureDialog::ConfigureDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::ConfigureDialog)
{
    ui->setupUi(this);
    this->setConfiguration();
}

ConfigureDialog::~ConfigureDialog() {
    delete ui;
}

void ConfigureDialog::setConfiguration() {
}

void ConfigureDialog::applyConfiguration() {
    Config config;
    ui->generalTab->applyConfiguration();
    ui->debugTab->applyConfiguration();
    config.Save();
}
