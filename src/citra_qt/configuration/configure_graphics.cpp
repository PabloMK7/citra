// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "citra_qt/configuration/configure_graphics.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_graphics.h"

ConfigureGraphics::ConfigureGraphics(QWidget* parent)
    : QWidget(parent), ui(new Ui::ConfigureGraphics) {

    ui->setupUi(this);
    this->setConfiguration();

    ui->toggle_vsync->setEnabled(!Core::System::GetInstance().IsPoweredOn());

    ui->layoutBox->setEnabled(!Settings::values.custom_layout);
}

ConfigureGraphics::~ConfigureGraphics() {}

enum class Resolution : int {
    Auto,
    Scale1x,
    Scale2x,
    Scale3x,
    Scale4x,
    Scale5x,
    Scale6x,
    Scale7x,
    Scale8x,
    Scale9x,
    Scale10x,
};

float ToResolutionFactor(Resolution option) {
    switch (option) {
    case Resolution::Auto:
        return 0.f;
    case Resolution::Scale1x:
        return 1.f;
    case Resolution::Scale2x:
        return 2.f;
    case Resolution::Scale3x:
        return 3.f;
    case Resolution::Scale4x:
        return 4.f;
    case Resolution::Scale5x:
        return 5.f;
    case Resolution::Scale6x:
        return 6.f;
    case Resolution::Scale7x:
        return 7.f;
    case Resolution::Scale8x:
        return 8.f;
    case Resolution::Scale9x:
        return 9.f;
    case Resolution::Scale10x:
        return 10.f;
    }
    return 0.f;
}

Resolution FromResolutionFactor(float factor) {
    if (factor == 0.f) {
        return Resolution::Auto;
    } else if (factor == 1.f) {
        return Resolution::Scale1x;
    } else if (factor == 2.f) {
        return Resolution::Scale2x;
    } else if (factor == 3.f) {
        return Resolution::Scale3x;
    } else if (factor == 4.f) {
        return Resolution::Scale4x;
    } else if (factor == 5.f) {
        return Resolution::Scale5x;
    } else if (factor == 6.f) {
        return Resolution::Scale6x;
    } else if (factor == 7.f) {
        return Resolution::Scale7x;
    } else if (factor == 8.f) {
        return Resolution::Scale8x;
    } else if (factor == 9.f) {
        return Resolution::Scale9x;
    } else if (factor == 10.f) {
        return Resolution::Scale10x;
    }
    return Resolution::Auto;
}

void ConfigureGraphics::setConfiguration() {
    ui->toggle_hw_renderer->setChecked(Settings::values.use_hw_renderer);
    ui->resolution_factor_combobox->setEnabled(Settings::values.use_hw_renderer);
    ui->toggle_shader_jit->setChecked(Settings::values.use_shader_jit);
    ui->resolution_factor_combobox->setCurrentIndex(
        static_cast<int>(FromResolutionFactor(Settings::values.resolution_factor)));
    ui->toggle_vsync->setChecked(Settings::values.use_vsync);
    ui->toggle_framelimit->setChecked(Settings::values.toggle_framelimit);
    ui->layout_combobox->setCurrentIndex(static_cast<int>(Settings::values.layout_option));
    ui->swap_screen->setChecked(Settings::values.swap_screen);
}

void ConfigureGraphics::applyConfiguration() {
    Settings::values.use_hw_renderer = ui->toggle_hw_renderer->isChecked();
    Settings::values.use_shader_jit = ui->toggle_shader_jit->isChecked();
    Settings::values.resolution_factor =
        ToResolutionFactor(static_cast<Resolution>(ui->resolution_factor_combobox->currentIndex()));
    Settings::values.use_vsync = ui->toggle_vsync->isChecked();
    Settings::values.toggle_framelimit = ui->toggle_framelimit->isChecked();
    Settings::values.layout_option =
        static_cast<Settings::LayoutOption>(ui->layout_combobox->currentIndex());
    Settings::values.swap_screen = ui->swap_screen->isChecked();
    Settings::Apply();
}
