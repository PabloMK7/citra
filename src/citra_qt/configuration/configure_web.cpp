// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/configuration/configure_web.h"
#include "core/settings.h"
#include "core/telemetry_session.h"
#include "ui_configure_web.h"

ConfigureWeb::ConfigureWeb(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureWeb>()) {
    ui->setupUi(this);
    connect(ui->button_regenerate_telemetry_id, &QPushButton::clicked, this,
            &ConfigureWeb::refreshTelemetryID);

    this->setConfiguration();
}

ConfigureWeb::~ConfigureWeb() {}

void ConfigureWeb::setConfiguration() {
    ui->web_credentials_disclaimer->setWordWrap(true);
    ui->telemetry_learn_more->setOpenExternalLinks(true);
    ui->telemetry_learn_more->setText("<a "
                                      "href='https://citra-emu.org/entry/"
                                      "telemetry-and-why-thats-a-good-thing/'>Learn more</a>");

    ui->web_signup_link->setOpenExternalLinks(true);
    ui->web_signup_link->setText("<a href='https://services.citra-emu.org/'>Sign up</a>");
    ui->web_token_info_link->setOpenExternalLinks(true);
    ui->web_token_info_link->setText(
        "<a href='https://citra-emu.org/wiki/citra-web-service/'>What is my token?</a>");

    ui->toggle_telemetry->setChecked(Settings::values.enable_telemetry);
    ui->edit_username->setText(QString::fromStdString(Settings::values.citra_username));
    ui->edit_token->setText(QString::fromStdString(Settings::values.citra_token));
    ui->label_telemetry_id->setText("Telemetry ID: 0x" +
                                    QString::number(Core::GetTelemetryId(), 16).toUpper());
}

void ConfigureWeb::applyConfiguration() {
    Settings::values.enable_telemetry = ui->toggle_telemetry->isChecked();
    Settings::values.citra_username = ui->edit_username->text().toStdString();
    Settings::values.citra_token = ui->edit_token->text().toStdString();
    Settings::Apply();
}

void ConfigureWeb::refreshTelemetryID() {
    const u64 new_telemetry_id{Core::RegenerateTelemetryId()};
    ui->label_telemetry_id->setText("Telemetry ID: 0x" +
                                    QString::number(new_telemetry_id, 16).toUpper());
}
