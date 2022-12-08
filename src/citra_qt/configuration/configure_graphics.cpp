// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QColorDialog>
#ifdef __APPLE__
#include <QMessageBox>
#endif
#include "citra_qt/configuration/configuration_shared.h"
#include "citra_qt/configuration/configure_graphics.h"
#include "common/settings.h"
#include "core/core.h"
#include "ui_configure_graphics.h"

ConfigureGraphics::ConfigureGraphics(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureGraphics>()) {
    ui->setupUi(this);

    SetupPerGameUI();
    SetConfiguration();

    ui->hw_renderer_group->setEnabled(ui->hw_renderer_group->isEnabled() &&
                                      ui->toggle_hw_renderer->isChecked());
    ui->toggle_vsync_new->setEnabled(!Core::System::GetInstance().IsPoweredOn());

    connect(ui->toggle_hw_renderer, &QCheckBox::toggled, this, [this] {
        const bool checked = ui->toggle_hw_renderer->isChecked();
        ui->hw_renderer_group->setEnabled(checked);
        ui->toggle_disk_shader_cache->setEnabled(checked && ui->toggle_hw_shader->isChecked());
    });

    ui->hw_shader_group->setEnabled(ui->toggle_hw_shader->isChecked());
    ui->toggle_disk_shader_cache->setEnabled(ui->toggle_hw_renderer->isChecked() &&
                                             ui->toggle_hw_shader->isChecked());

    connect(ui->toggle_hw_shader, &QCheckBox::toggled, this, [this] {
        const bool checked = ui->toggle_hw_shader->isChecked();
        ui->hw_shader_group->setEnabled(checked);
        ui->toggle_disk_shader_cache->setEnabled(checked);
    });

#ifdef __APPLE__
    connect(ui->toggle_hw_shader, &QCheckBox::stateChanged, this, [this](int state) {
        if (state == Qt::Checked) {
            ui->toggle_separable_shader->setEnabled(true);
        }
    });
    connect(ui->toggle_separable_shader, &QCheckBox::stateChanged, this, [this](int state) {
        if (state == Qt::Checked) {
            QMessageBox::warning(
                this, tr("Hardware Shader Warning"),
                tr("Separable Shader support is broken on macOS with Intel GPUs, and will cause "
                   "graphical issues "
                   "like showing a black screen.<br><br>The option is only there for "
                   "test/development purposes. If you experience graphical issues with Hardware "
                   "Shader, please turn it off."));
        }
    });
#else
    // TODO(B3N30): Hide this for macs with none Intel GPUs, too.
    ui->toggle_separable_shader->setVisible(false);
#endif
}

ConfigureGraphics::~ConfigureGraphics() = default;

void ConfigureGraphics::SetConfiguration() {
    ui->toggle_hw_renderer->setChecked(Settings::values.use_hw_renderer.GetValue());
    ui->toggle_hw_shader->setChecked(Settings::values.use_hw_shader.GetValue());
    ui->toggle_separable_shader->setChecked(Settings::values.separable_shader.GetValue());
    ui->toggle_accurate_mul->setChecked(Settings::values.shaders_accurate_mul.GetValue());
    ui->toggle_disk_shader_cache->setChecked(Settings::values.use_disk_shader_cache.GetValue());
    ui->toggle_vsync_new->setChecked(Settings::values.use_vsync_new.GetValue());

    if (Settings::IsConfiguringGlobal()) {
        ui->toggle_shader_jit->setChecked(Settings::values.use_shader_jit.GetValue());
    }
}

void ConfigureGraphics::ApplyConfiguration() {
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_hw_renderer,
                                             ui->toggle_hw_renderer, use_hw_renderer);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_hw_shader, ui->toggle_hw_shader,
                                             use_hw_shader);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.separable_shader,
                                             ui->toggle_separable_shader, separable_shader);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.shaders_accurate_mul,
                                             ui->toggle_accurate_mul, shaders_accurate_mul);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_disk_shader_cache,
                                             ui->toggle_disk_shader_cache, use_disk_shader_cache);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_vsync_new, ui->toggle_vsync_new,
                                             use_vsync_new);

    if (Settings::IsConfiguringGlobal()) {
        Settings::values.use_shader_jit = ui->toggle_shader_jit->isChecked();
    }
}

void ConfigureGraphics::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureGraphics::SetupPerGameUI() {
    // Block the global settings if a game is currently running that overrides them
    if (Settings::IsConfiguringGlobal()) {
        ui->toggle_hw_renderer->setEnabled(Settings::values.use_hw_renderer.UsingGlobal());
        ui->toggle_hw_shader->setEnabled(Settings::values.use_hw_shader.UsingGlobal());
        ui->toggle_separable_shader->setEnabled(Settings::values.separable_shader.UsingGlobal());
        ui->toggle_accurate_mul->setEnabled(Settings::values.shaders_accurate_mul.UsingGlobal());
        ui->toggle_disk_shader_cache->setEnabled(
            Settings::values.use_disk_shader_cache.UsingGlobal());
        ui->toggle_vsync_new->setEnabled(Settings::values.use_vsync_new.UsingGlobal());
        return;
    }

    ui->toggle_shader_jit->setVisible(false);

    ConfigurationShared::SetColoredTristate(ui->toggle_hw_renderer,
                                            Settings::values.use_hw_renderer, use_hw_renderer);
    ConfigurationShared::SetColoredTristate(ui->toggle_hw_shader, Settings::values.use_hw_shader,
                                            use_hw_shader);
    ConfigurationShared::SetColoredTristate(ui->toggle_separable_shader,
                                            Settings::values.separable_shader, separable_shader);
    ConfigurationShared::SetColoredTristate(
        ui->toggle_accurate_mul, Settings::values.shaders_accurate_mul, shaders_accurate_mul);
    ConfigurationShared::SetColoredTristate(ui->toggle_disk_shader_cache,
                                            Settings::values.use_disk_shader_cache,
                                            use_disk_shader_cache);
    ConfigurationShared::SetColoredTristate(ui->toggle_vsync_new, Settings::values.use_vsync_new,
                                            use_vsync_new);
}
