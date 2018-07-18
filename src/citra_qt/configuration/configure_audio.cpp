// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include "audio_core/sink.h"
#include "audio_core/sink_details.h"
#include "citra_qt/configuration/configure_audio.h"
#include "core/settings.h"
#include "ui_configure_audio.h"

ConfigureAudio::ConfigureAudio(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureAudio>()) {
    ui->setupUi(this);

    ui->output_sink_combo_box->clear();
    ui->output_sink_combo_box->addItem("auto");
    for (const auto& sink_detail : AudioCore::g_sink_details) {
        ui->output_sink_combo_box->addItem(sink_detail.id);
    }

    connect(ui->volume_slider, &QSlider::valueChanged, [this] {
        ui->volume_indicator->setText(tr("%1 %").arg(ui->volume_slider->sliderPosition()));
    });

    this->setConfiguration();
    connect(ui->output_sink_combo_box,
            static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged), this,
            &ConfigureAudio::updateAudioDevices);
}

ConfigureAudio::~ConfigureAudio() {}

void ConfigureAudio::setConfiguration() {
    int new_sink_index = 0;
    for (int index = 0; index < ui->output_sink_combo_box->count(); index++) {
        if (ui->output_sink_combo_box->itemText(index).toStdString() == Settings::values.sink_id) {
            new_sink_index = index;
            break;
        }
    }
    ui->output_sink_combo_box->setCurrentIndex(new_sink_index);

    ui->toggle_audio_stretching->setChecked(Settings::values.enable_audio_stretching);

    // The device list cannot be pre-populated (nor listed) until the output sink is known.
    updateAudioDevices(new_sink_index);

    int new_device_index = -1;
    for (int index = 0; index < ui->audio_device_combo_box->count(); index++) {
        if (ui->audio_device_combo_box->itemText(index).toStdString() ==
            Settings::values.audio_device_id) {
            new_device_index = index;
            break;
        }
    }
    ui->audio_device_combo_box->setCurrentIndex(new_device_index);

    ui->volume_slider->setValue(Settings::values.volume * ui->volume_slider->maximum());
    ui->volume_indicator->setText(tr("%1 %").arg(ui->volume_slider->sliderPosition()));
}

void ConfigureAudio::applyConfiguration() {
    Settings::values.sink_id =
        ui->output_sink_combo_box->itemText(ui->output_sink_combo_box->currentIndex())
            .toStdString();
    Settings::values.enable_audio_stretching = ui->toggle_audio_stretching->isChecked();
    Settings::values.audio_device_id =
        ui->audio_device_combo_box->itemText(ui->audio_device_combo_box->currentIndex())
            .toStdString();
    Settings::values.volume =
        static_cast<float>(ui->volume_slider->sliderPosition()) / ui->volume_slider->maximum();
}

void ConfigureAudio::updateAudioDevices(int sink_index) {
    ui->audio_device_combo_box->clear();
    ui->audio_device_combo_box->addItem(AudioCore::auto_device_name);

    std::string sink_id = ui->output_sink_combo_box->itemText(sink_index).toStdString();
    std::vector<std::string> device_list = AudioCore::GetSinkDetails(sink_id).list_devices();
    for (const auto& device : device_list) {
        ui->audio_device_combo_box->addItem(device.c_str());
    }
}

void ConfigureAudio::retranslateUi() {
    ui->retranslateUi(this);
}
