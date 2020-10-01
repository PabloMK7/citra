// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QMessageBox>
#include "citra_qt/configuration/configure_general.h"
#include "citra_qt/uisettings.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_general.h"

// The QSlider doesn't have an easy way to set a custom step amount,
// so we can just convert from the sliders range (0 - 198) to the expected
// settings range (5 - 995) with simple math.
static constexpr int SliderToSettings(int value) {
    return 5 * value + 5;
}

static constexpr int SettingsToSlider(int value) {
    return (value - 5) / 5;
}

ConfigureGeneral::ConfigureGeneral(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureGeneral>()) {

    ui->setupUi(this);

    // Set a minimum width for the label to prevent the slider from changing size.
    // This scales across DPIs, and is acceptable for uncapitalized strings.
    ui->emulation_speed_display_label->setMinimumWidth(tr("unthrottled").size() * 6);

    SetConfiguration();

    ui->updateBox->setVisible(UISettings::values.updater_found);
    connect(ui->button_reset_defaults, &QPushButton::clicked, this,
            &ConfigureGeneral::ResetDefaults);

    connect(ui->frame_limit, &QSlider::valueChanged, [&](int value) {
        if (value == ui->frame_limit->maximum()) {
            ui->emulation_speed_display_label->setText(tr("unthrottled"));
        } else {
            ui->emulation_speed_display_label->setText(
                QStringLiteral("%1%")
                    .arg(SliderToSettings(value))
                    .rightJustified(tr("unthrottled").size()));
        }
    });

    connect(ui->frame_limit_alternate, &QSlider::valueChanged, [&](int value) {
        if (value == ui->frame_limit_alternate->maximum()) {
            ui->emulation_speed_alternate_display_label->setText(tr("unthrottled"));
        } else {
            ui->emulation_speed_alternate_display_label->setText(
                QStringLiteral("%1%")
                    .arg(SliderToSettings(value))
                    .rightJustified(tr("unthrottled").size()));
        }
    });
}

ConfigureGeneral::~ConfigureGeneral() = default;

void ConfigureGeneral::SetConfiguration() {
    ui->toggle_check_exit->setChecked(UISettings::values.confirm_before_closing);
    ui->toggle_background_pause->setChecked(UISettings::values.pause_when_in_background);
    ui->toggle_hide_mouse->setChecked(UISettings::values.hide_mouse);

    ui->toggle_update_check->setChecked(UISettings::values.check_for_update_on_start);
    ui->toggle_auto_update->setChecked(UISettings::values.update_on_close);

    // The first item is "auto-select" with actual value -1, so plus one here will do the trick
    ui->region_combobox->setCurrentIndex(Settings::values.region_value + 1);

    if (Settings::values.frame_limit == 0) {
        ui->frame_limit->setValue(ui->frame_limit->maximum());
    } else {
        ui->frame_limit->setValue(SettingsToSlider(Settings::values.frame_limit));
    }
    if (ui->frame_limit->value() == ui->frame_limit->maximum()) {
        ui->emulation_speed_display_label->setText(tr("unthrottled"));
    } else {
        ui->emulation_speed_display_label->setText(
            QStringLiteral("%1%")
                .arg(SliderToSettings(ui->frame_limit->value()))
                .rightJustified(tr("unthrottled").size()));
    }

    ui->toggle_alternate_speed->setChecked(Settings::values.use_frame_limit_alternate);

    if (Settings::values.frame_limit_alternate == 0) {
        ui->frame_limit_alternate->setValue(ui->frame_limit_alternate->maximum());
    } else {
        ui->frame_limit_alternate->setValue(
            SettingsToSlider(Settings::values.frame_limit_alternate));
    }
    if (ui->frame_limit_alternate->value() == ui->frame_limit_alternate->maximum()) {
        ui->emulation_speed_alternate_display_label->setText(tr("unthrottled"));
    } else {
        ui->emulation_speed_alternate_display_label->setText(
            QStringLiteral("%1%")
                .arg(SliderToSettings(ui->frame_limit_alternate->value()))
                .rightJustified(tr("unthrottled").size()));
    }
}

void ConfigureGeneral::ResetDefaults() {
    QMessageBox::StandardButton answer = QMessageBox::question(
        this, tr("Citra"),
        tr("Are you sure you want to <b>reset your settings</b> and close Citra?"),
        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);

    if (answer == QMessageBox::No)
        return;

    FileUtil::Delete(FileUtil::GetUserPath(FileUtil::UserPath::ConfigDir) + "qt-config.ini");
    std::exit(0);
}

void ConfigureGeneral::ApplyConfiguration() {
    UISettings::values.confirm_before_closing = ui->toggle_check_exit->isChecked();
    UISettings::values.pause_when_in_background = ui->toggle_background_pause->isChecked();
    UISettings::values.hide_mouse = ui->toggle_hide_mouse->isChecked();

    UISettings::values.check_for_update_on_start = ui->toggle_update_check->isChecked();
    UISettings::values.update_on_close = ui->toggle_auto_update->isChecked();

    Settings::values.region_value = ui->region_combobox->currentIndex() - 1;

    if (ui->frame_limit->value() == ui->frame_limit->maximum()) {
        Settings::values.frame_limit = 0;
    } else {
        Settings::values.frame_limit = SliderToSettings(ui->frame_limit->value());
    }
    Settings::values.use_frame_limit_alternate = ui->toggle_alternate_speed->isChecked();
    if (ui->frame_limit_alternate->value() == ui->frame_limit_alternate->maximum()) {
        Settings::values.frame_limit_alternate = 0;
    } else {
        Settings::values.frame_limit_alternate =
            SliderToSettings(ui->frame_limit_alternate->value());
    }
}

void ConfigureGeneral::RetranslateUI() {
    ui->retranslateUi(this);
}
