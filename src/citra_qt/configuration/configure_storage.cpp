// Copyright 2021 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDesktopServices>
#include <QFileDialog>
#include <QUrl>
#include "citra_qt/configuration/configure_storage.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_storage.h"

ConfigureStorage::ConfigureStorage(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureStorage>()) {
    ui->setupUi(this);
    SetConfiguration();

    connect(ui->open_nand_dir, &QPushButton::clicked, []() {
        QString path = QString::fromStdString(Settings::values.nand_dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    connect(ui->change_nand_dir, &QPushButton::clicked, this, [this]() {
        const QString dir_path = QFileDialog::getExistingDirectory(
            this, tr("Select NAND Directory"), QString::fromStdString(Settings::values.nand_dir),
            QFileDialog::ShowDirsOnly);
        if (!dir_path.isEmpty()) {
            Settings::values.nand_dir = dir_path.toStdString();
            SetConfiguration();
        }
    });

    connect(ui->open_sdmc_dir, &QPushButton::clicked, []() {
        QString path = QString::fromStdString(Settings::values.sdmc_dir);
        QDesktopServices::openUrl(QUrl::fromLocalFile(path));
    });

    connect(ui->change_sdmc_dir, &QPushButton::clicked, this, [this]() {
        const QString dir_path = QFileDialog::getExistingDirectory(
            this, tr("Select SDMC Directory"), QString::fromStdString(Settings::values.sdmc_dir),
            QFileDialog::ShowDirsOnly);
        if (!dir_path.isEmpty()) {
            Settings::values.sdmc_dir = dir_path.toStdString();
            SetConfiguration();
        }
    });

    connect(ui->toggle_virtual_sd, &QCheckBox::clicked, this, [this]() {
        ApplyConfiguration();
        SetConfiguration();
    });
}

ConfigureStorage::~ConfigureStorage() = default;

void ConfigureStorage::SetConfiguration() {
    ui->nand_group->setVisible(Settings::values.use_virtual_sd);
    QString nand_path = QString::fromStdString(Settings::values.nand_dir);
    ui->nand_dir_path->setText(nand_path);
    ui->open_nand_dir->setEnabled(!Settings::values.nand_dir.empty());

    ui->sdmc_group->setVisible(Settings::values.use_virtual_sd);
    QString sdmc_path = QString::fromStdString(Settings::values.sdmc_dir);
    ui->sdmc_dir_path->setText(sdmc_path);
    ui->open_sdmc_dir->setEnabled(!Settings::values.sdmc_dir.empty());

    ui->toggle_virtual_sd->setChecked(Settings::values.use_virtual_sd);

    ui->storage_group->setEnabled(!Core::System::GetInstance().IsPoweredOn());
}

void ConfigureStorage::ApplyConfiguration() {
    Settings::values.use_virtual_sd = ui->toggle_virtual_sd->isChecked();
}

void ConfigureStorage::RetranslateUI() {
    ui->retranslateUi(this);
}
