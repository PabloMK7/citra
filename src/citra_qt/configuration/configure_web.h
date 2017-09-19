// Copyright 2017 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <future>
#include <memory>
#include <QWidget>

namespace Ui {
class ConfigureWeb;
}

class ConfigureWeb : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureWeb(QWidget* parent = nullptr);
    ~ConfigureWeb();

    void applyConfiguration();

public slots:
    void RefreshTelemetryID();
    void OnLoginChanged();
    void VerifyLogin();
    void OnLoginVerified();

signals:
    void LoginVerified();

private:
    void setConfiguration();

    bool user_verified = true;
    std::future<bool> verified;

    std::unique_ptr<Ui::ConfigureWeb> ui;
};
