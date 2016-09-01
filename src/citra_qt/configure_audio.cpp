// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "audio_core/sink_details.h"

#include "citra_qt/configure_audio.h"
#include "ui_configure_audio.h"

#include "core/settings.h"

ConfigureAudio::ConfigureAudio(QWidget* parent) :
        QWidget(parent),
        ui(std::make_unique<Ui::ConfigureAudio>())
{
    ui->setupUi(this);

    ui->output_sink_combo_box->clear();
    ui->output_sink_combo_box->addItem("auto");
    for (const auto& sink_detail : AudioCore::g_sink_details) {
        ui->output_sink_combo_box->addItem(sink_detail.id);
    }

    this->setConfiguration();
}

ConfigureAudio::~ConfigureAudio() {
}

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
}

void ConfigureAudio::applyConfiguration() {
    Settings::values.sink_id = ui->output_sink_combo_box->itemText(ui->output_sink_combo_box->currentIndex()).toStdString();
    Settings::values.enable_audio_stretching = ui->toggle_audio_stretching->isChecked();
    Settings::Apply();
}
