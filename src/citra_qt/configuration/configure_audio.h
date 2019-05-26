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
    ~ConfigureAudio() override;

    void ApplyConfiguration();
    void RetranslateUI();
    void SetConfiguration();

private:
    void UpdateAudioOutputDevices(int sink_index);
    void UpdateAudioInputDevices(int index);

    void SetOutputSinkFromSinkID();
    void SetAudioDeviceFromDeviceID();
    void SetVolumeIndicatorText(int percentage);

    std::unique_ptr<Ui::ConfigureAudio> ui;
};
