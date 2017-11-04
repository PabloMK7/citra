// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "aboutdialog.h"
#include "common/scm_rev.h"
#include "ui_aboutdialog.h"

AboutDialog::AboutDialog(QWidget* parent)
    : QDialog(parent, Qt::WindowTitleHint | Qt::WindowCloseButtonHint | Qt::WindowSystemMenuHint),
      ui(new Ui::AboutDialog) {
    ui->setupUi(this);
    ui->labelBuildInfo->setText(ui->labelBuildInfo->text().arg(
        Common::g_build_name, Common::g_scm_branch, Common::g_scm_desc));
}

AboutDialog::~AboutDialog() {
    delete ui;
}
