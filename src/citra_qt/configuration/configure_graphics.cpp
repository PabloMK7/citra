// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QColorDialog>
#ifdef __APPLE__
#include <QMessageBox>
#endif
#include "citra_qt/configuration/configure_graphics.h"
#include "core/core.h"
#include "core/settings.h"
#include "ui_configure_graphics.h"
#include "video_core/renderer_opengl/post_processing_opengl.h"

ConfigureGraphics::ConfigureGraphics(QWidget* parent)
    : QWidget(parent), ui(new Ui::ConfigureGraphics) {
    ui->setupUi(this);
    SetConfiguration();

    ui->hw_renderer_group->setEnabled(ui->toggle_hw_renderer->isChecked());
    ui->toggle_vsync_new->setEnabled(!Core::System::GetInstance().IsPoweredOn());

    connect(ui->toggle_hw_renderer, &QCheckBox::toggled, this, [this] {
        auto checked = ui->toggle_hw_renderer->isChecked();
        ui->hw_renderer_group->setEnabled(checked);
    });

    ui->hw_shader_group->setEnabled(ui->toggle_hw_shader->isChecked());
    connect(ui->toggle_hw_shader, &QCheckBox::toggled, ui->hw_shader_group, &QWidget::setEnabled);
#ifdef __APPLE__
    connect(ui->toggle_hw_shader, &QCheckBox::stateChanged, this, [this](int state) {
        if (state == Qt::Checked) {
            QMessageBox::warning(
                this, tr("Hardware Shader Warning"),
                tr("Hardware Shader support is broken on macOS, and will cause graphical issues "
                   "like showing a black screen.<br><br>The option is only there for "
                   "test/development purposes. If you experience graphical issues with Hardware "
                   "Shader, please turn it off."));
        }
    });
#endif
}

ConfigureGraphics::~ConfigureGraphics() = default;

void ConfigureGraphics::SetConfiguration() {
    ui->toggle_hw_renderer->setChecked(Settings::values.use_hw_renderer);
    ui->toggle_hw_shader->setChecked(Settings::values.use_hw_shader);
    ui->toggle_accurate_mul->setChecked(Settings::values.shaders_accurate_mul);
    ui->toggle_shader_jit->setChecked(Settings::values.use_shader_jit);
    ui->toggle_vsync_new->setChecked(Settings::values.use_vsync_new);
}

void ConfigureGraphics::ApplyConfiguration() {
    Settings::values.use_hw_renderer = ui->toggle_hw_renderer->isChecked();
    Settings::values.use_hw_shader = ui->toggle_hw_shader->isChecked();
    Settings::values.shaders_accurate_mul = ui->toggle_accurate_mul->isChecked();
    Settings::values.use_shader_jit = ui->toggle_shader_jit->isChecked();
    Settings::values.use_vsync_new = ui->toggle_vsync_new->isChecked();
}

void ConfigureGraphics::RetranslateUI() {
    ui->retranslateUi(this);
}
