// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class ConfigureGraphics;
}

class ConfigureGraphics : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureGraphics(QWidget* parent = nullptr);
    ~ConfigureGraphics() override;

    void ApplyConfiguration();
    void RetranslateUI();
    void SetConfiguration();

    std::unique_ptr<Ui::ConfigureGraphics> ui;
    QColor bg_color;

private:
    void updateShaders(bool anaglyph);
};
