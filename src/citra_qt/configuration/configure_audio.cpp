// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <QtGlobal>
#ifdef HAVE_CUBEB
#include "audio_core/cubeb_input.h"
#endif
#include "audio_core/sink.h"
#include "audio_core/sink_details.h"
#include "citra_qt/configuration/configure_audio.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_audio.h"

ConfigureAudio::ConfigureAudio(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureAudio>()) {
    ui->setupUi(this);

    ui->output_sink_combo_box->clear();
    ui->output_sink_combo_box->addItem("auto");
    for (const char* id : AudioCore::GetSinkIDs()) {
        ui->output_sink_combo_box->addItem(id);
    }

    ui->emulation_combo_box->addItem(tr("HLE (fast)"));
    ui->emulation_combo_box->addItem(tr("LLE (accurate)"));
    ui->emulation_combo_box->addItem(tr("LLE multi-core"));
    ui->emulation_combo_box->setEnabled(!Core::System::GetInstance().IsPoweredOn());

    connect(ui->volume_slider, &QSlider::valueChanged, this,
            &ConfigureAudio::SetVolumeIndicatorText);

    ui->input_device_combo_box->clear();
    ui->input_device_combo_box->addItem(tr("Default"));
#ifdef HAVE_CUBEB
    for (const auto& device : AudioCore::ListCubebInputDevices()) {
        ui->input_device_combo_box->addItem(QString::fromStdString(device));
    }
#endif
    connect(ui->input_type_combo_box, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ConfigureAudio::UpdateAudioInputDevices);

    SetConfiguration();
    connect(ui->output_sink_combo_box, qOverload<int>(&QComboBox::currentIndexChanged), this,
            &ConfigureAudio::UpdateAudioOutputDevices);
}

ConfigureAudio::~ConfigureAudio() {}

void ConfigureAudio::SetConfiguration() {
    SetOutputSinkFromSinkID();

    // The device list cannot be pre-populated (nor listed) until the output sink is known.
    UpdateAudioOutputDevices(ui->output_sink_combo_box->currentIndex());

    SetAudioDeviceFromDeviceID();

    ui->toggle_audio_stretching->setChecked(Settings::values.enable_audio_stretching);
    ui->volume_slider->setValue(Settings::values.volume * ui->volume_slider->maximum());
    SetVolumeIndicatorText(ui->volume_slider->sliderPosition());

    int selection;
    if (Settings::values.enable_dsp_lle) {
        if (Settings::values.enable_dsp_lle_multithread) {
            selection = 2;
        } else {
            selection = 1;
        }
    } else {
        selection = 0;
    }
    ui->emulation_combo_box->setCurrentIndex(selection);

    int index = static_cast<int>(Settings::values.mic_input_type);
    ui->input_type_combo_box->setCurrentIndex(index);
    ui->input_device_combo_box->setCurrentText(
        QString::fromStdString(Settings::values.mic_input_device));
    UpdateAudioInputDevices(index);
}

void ConfigureAudio::SetOutputSinkFromSinkID() {
    int new_sink_index = 0;

    const QString sink_id = QString::fromStdString(Settings::values.sink_id);
    for (int index = 0; index < ui->output_sink_combo_box->count(); index++) {
        if (ui->output_sink_combo_box->itemText(index) == sink_id) {
            new_sink_index = index;
            break;
        }
    }

    ui->output_sink_combo_box->setCurrentIndex(new_sink_index);
}

void ConfigureAudio::SetAudioDeviceFromDeviceID() {
    int new_device_index = -1;

    const QString device_id = QString::fromStdString(Settings::values.audio_device_id);
    for (int index = 0; index < ui->audio_device_combo_box->count(); index++) {
        if (ui->audio_device_combo_box->itemText(index) == device_id) {
            new_device_index = index;
            break;
        }
    }

    ui->audio_device_combo_box->setCurrentIndex(new_device_index);
}

void ConfigureAudio::SetVolumeIndicatorText(int percentage) {
    ui->volume_indicator->setText(tr("%1%", "Volume percentage (e.g. 50%)").arg(percentage));
}

void ConfigureAudio::ApplyConfiguration() {
    Settings::values.sink_id =
        ui->output_sink_combo_box->itemText(ui->output_sink_combo_box->currentIndex())
            .toStdString();
    Settings::values.enable_audio_stretching = ui->toggle_audio_stretching->isChecked();
    Settings::values.audio_device_id =
        ui->audio_device_combo_box->itemText(ui->audio_device_combo_box->currentIndex())
            .toStdString();
    Settings::values.volume =
        static_cast<float>(ui->volume_slider->sliderPosition()) / ui->volume_slider->maximum();
    Settings::values.enable_dsp_lle = ui->emulation_combo_box->currentIndex() != 0;
    Settings::values.enable_dsp_lle_multithread = ui->emulation_combo_box->currentIndex() == 2;
    Settings::values.mic_input_type =
        static_cast<Settings::MicInputType>(ui->input_type_combo_box->currentIndex());
    Settings::values.mic_input_device = ui->input_device_combo_box->currentText().toStdString();
}

void ConfigureAudio::UpdateAudioOutputDevices(int sink_index) {
    ui->audio_device_combo_box->clear();
    ui->audio_device_combo_box->addItem(AudioCore::auto_device_name);

    const std::string sink_id = ui->output_sink_combo_box->itemText(sink_index).toStdString();
    for (const auto& device : AudioCore::GetDeviceListForSink(sink_id)) {
        ui->audio_device_combo_box->addItem(QString::fromStdString(device));
    }
}

void ConfigureAudio::UpdateAudioInputDevices(int index) {}

void ConfigureAudio::RetranslateUI() {
    ui->retranslateUi(this);
}
