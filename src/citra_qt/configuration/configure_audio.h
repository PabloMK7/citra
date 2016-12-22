// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QWidget>

namespace Ui {
class ConfigureAudio;
}

class ConfigureAudio : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureAudio(QWidget* parent = nullptr);
    ~ConfigureAudio();

    void applyConfiguration();

public slots:
    void updateAudioDevices(int sink_index);

private:
    void setConfiguration();

    std::unique_ptr<Ui::ConfigureAudio> ui;
};
