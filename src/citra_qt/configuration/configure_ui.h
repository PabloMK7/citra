// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class ConfigureUi;
}

class ConfigureUi : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureUi(QWidget* parent = nullptr);
    ~ConfigureUi() override;

    void applyConfiguration();
    void retranslateUi();
    void setConfiguration();

private slots:
    void onLanguageChanged(int index);

signals:
    void languageChanged(const QString& locale);

private:
    std::unique_ptr<Ui::ConfigureUi> ui;
};
