// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QIcon>
#include <QMessageBox>
#include <QtConcurrent/QtConcurrentRun>
#include "citra_qt/configuration/configure_web.h"
#include "citra_qt/ui_settings.h"
#include "core/settings.h"
#include "core/telemetry_session.h"
#include "ui_configure_web.h"

ConfigureWeb::ConfigureWeb(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureWeb>()) {
    ui->setupUi(this);
    connect(ui->button_regenerate_telemetry_id, &QPushButton::clicked, this,
            &ConfigureWeb::RefreshTelemetryID);
    connect(ui->button_verify_login, &QPushButton::clicked, this, &ConfigureWeb::VerifyLogin);
    connect(&verify_watcher, &QFutureWatcher<bool>::finished, this, &ConfigureWeb::OnLoginVerified);

#ifndef USE_DISCORD_PRESENCE
    ui->discord_group->setVisible(false);
#endif
    this->setConfiguration();
}

ConfigureWeb::~ConfigureWeb() = default;

void ConfigureWeb::setConfiguration() {
    ui->web_credentials_disclaimer->setWordWrap(true);
    ui->telemetry_learn_more->setOpenExternalLinks(true);
    ui->telemetry_learn_more->setText(tr("<a "
                                         "href='https://citra-emu.org/entry/"
                                         "telemetry-and-why-thats-a-good-thing/'><span "
                                         "style=\"text-decoration: underline; "
                                         "color:#039be5;\">Learn more</span></a>"));

    ui->web_signup_link->setOpenExternalLinks(true);
    ui->web_signup_link->setText(
        tr("<a href='https://profile.citra-emu.org/'><span style=\"text-decoration: underline; "
           "color:#039be5;\">Sign up</span></a>"));
    ui->web_token_info_link->setOpenExternalLinks(true);
    ui->web_token_info_link->setText(
        tr("<a href='https://citra-emu.org/wiki/citra-web-service/'><span style=\"text-decoration: "
           "underline; color:#039be5;\">What is my token?</span></a>"));

    ui->toggle_telemetry->setChecked(Settings::values.enable_telemetry);
    ui->edit_username->setText(QString::fromStdString(Settings::values.citra_username));
    ui->edit_token->setText(QString::fromStdString(Settings::values.citra_token));
    // Connect after setting the values, to avoid calling OnLoginChanged now
    connect(ui->edit_token, &QLineEdit::textChanged, this, &ConfigureWeb::OnLoginChanged);
    connect(ui->edit_username, &QLineEdit::textChanged, this, &ConfigureWeb::OnLoginChanged);
    ui->label_telemetry_id->setText(
        tr("Telemetry ID: 0x%1").arg(QString::number(Core::GetTelemetryId(), 16).toUpper()));
    user_verified = true;

    ui->toggle_discordrpc->setChecked(UISettings::values.enable_discord_presence);
}

void ConfigureWeb::applyConfiguration() {
    Settings::values.enable_telemetry = ui->toggle_telemetry->isChecked();
    UISettings::values.enable_discord_presence = ui->toggle_discordrpc->isChecked();
    if (user_verified) {
        Settings::values.citra_username = ui->edit_username->text().toStdString();
        Settings::values.citra_token = ui->edit_token->text().toStdString();
    } else {
        QMessageBox::warning(this, tr("Username and token not verified"),
                             tr("Username and token were not verified. The changes to your "
                                "username and/or token have not been saved."));
    }
}

void ConfigureWeb::RefreshTelemetryID() {
    const u64 new_telemetry_id{Core::RegenerateTelemetryId()};
    ui->label_telemetry_id->setText(
        tr("Telemetry ID: 0x%1").arg(QString::number(new_telemetry_id, 16).toUpper()));
}

void ConfigureWeb::OnLoginChanged() {
    if (ui->edit_username->text().isEmpty() && ui->edit_token->text().isEmpty()) {
        user_verified = true;
        ui->label_username_verified->setPixmap(QIcon::fromTheme("checked").pixmap(16));
        ui->label_token_verified->setPixmap(QIcon::fromTheme("checked").pixmap(16));
    } else {
        user_verified = false;
        ui->label_username_verified->setPixmap(QIcon::fromTheme("failed").pixmap(16));
        ui->label_token_verified->setPixmap(QIcon::fromTheme("failed").pixmap(16));
    }
}

void ConfigureWeb::VerifyLogin() {
    ui->button_verify_login->setDisabled(true);
    ui->button_verify_login->setText(tr("Verifying"));
    verify_watcher.setFuture(
        QtConcurrent::run([this, username = ui->edit_username->text().toStdString(),
                           token = ui->edit_token->text().toStdString()]() {
            return Core::VerifyLogin(username, token);
        }));
}

void ConfigureWeb::OnLoginVerified() {
    ui->button_verify_login->setEnabled(true);
    ui->button_verify_login->setText(tr("Verify"));
    if (verify_watcher.result()) {
        user_verified = true;
        ui->label_username_verified->setPixmap(QIcon::fromTheme("checked").pixmap(16));
        ui->label_token_verified->setPixmap(QIcon::fromTheme("checked").pixmap(16));
    } else {
        ui->label_username_verified->setPixmap(QIcon::fromTheme("failed").pixmap(16));
        ui->label_token_verified->setPixmap(QIcon::fromTheme("failed").pixmap(16));
        QMessageBox::critical(
            this, tr("Verification failed"),
            tr("Verification failed. Check that you have entered your username and token "
               "correctly, and that your internet connection is working."));
    }
}

void ConfigureWeb::retranslateUi() {
    ui->retranslateUi(this);
}
