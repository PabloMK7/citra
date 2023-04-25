// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QColorDialog>
#include "citra_qt/configuration/configuration_shared.h"
#include "citra_qt/configuration/configure_graphics.h"
#include "common/settings.h"
#include "core/core.h"
#include "ui_configure_graphics.h"

ConfigureGraphics::ConfigureGraphics(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureGraphics>()) {
    ui->setupUi(this);

    ui->toggle_vsync_new->setEnabled(!Core::System::GetInstance().IsPoweredOn());
    // Set the index to -1 to ensure the below lambda is called with setCurrentIndex
    ui->graphics_api_combo->setCurrentIndex(-1);

    connect(ui->graphics_api_combo, qOverload<int>(&QComboBox::currentIndexChanged), this,
            [this](int index) {
                const auto graphics_api =
                    ConfigurationShared::GetComboboxSetting(index, &Settings::values.graphics_api);
                const bool is_software = graphics_api == Settings::GraphicsAPI::Software;

                ui->hw_renderer_group->setEnabled(!is_software);
                ui->toggle_disk_shader_cache->setEnabled(!is_software &&
                                                         ui->toggle_hw_shader->isChecked());
            });

    connect(ui->toggle_hw_shader, &QCheckBox::toggled, this, [this] {
        const bool checked = ui->toggle_hw_shader->isChecked();
        ui->hw_shader_group->setEnabled(checked);
        ui->toggle_disk_shader_cache->setEnabled(checked);
    });

    SetupPerGameUI();
    SetConfiguration();
}

ConfigureGraphics::~ConfigureGraphics() = default;

void ConfigureGraphics::SetConfiguration() {
    if (!Settings::IsConfiguringGlobal()) {
        ConfigurationShared::SetHighlight(ui->graphics_api_group,
                                          !Settings::values.graphics_api.UsingGlobal());
        ConfigurationShared::SetPerGameSetting(ui->graphics_api_combo,
                                               &Settings::values.graphics_api);
    } else {
        ui->graphics_api_combo->setCurrentIndex(
            static_cast<int>(Settings::values.graphics_api.GetValue()));
    }

    ui->toggle_hw_shader->setChecked(Settings::values.use_hw_shader.GetValue());
    ui->toggle_accurate_mul->setChecked(Settings::values.shaders_accurate_mul.GetValue());
    ui->toggle_disk_shader_cache->setChecked(Settings::values.use_disk_shader_cache.GetValue());
    ui->toggle_vsync_new->setChecked(Settings::values.use_vsync_new.GetValue());

    if (Settings::IsConfiguringGlobal()) {
        ui->toggle_shader_jit->setChecked(Settings::values.use_shader_jit.GetValue());
    }
}

void ConfigureGraphics::ApplyConfiguration() {
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.graphics_api,
                                             ui->graphics_api_combo);
    ConfigurationShared::ApplyPerGameSetting(&Settings::values.use_hw_shader, ui->toggle_hw_shader,
                                             use_hw_shader);
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
        ui->graphics_api_group->setEnabled(Settings::values.graphics_api.UsingGlobal());
        ui->toggle_hw_shader->setEnabled(Settings::values.use_hw_shader.UsingGlobal());
        ui->toggle_accurate_mul->setEnabled(Settings::values.shaders_accurate_mul.UsingGlobal());
        ui->toggle_disk_shader_cache->setEnabled(
            Settings::values.use_disk_shader_cache.UsingGlobal());
        ui->toggle_vsync_new->setEnabled(ui->toggle_vsync_new->isEnabled() &&
                                         Settings::values.use_vsync_new.UsingGlobal());
        return;
    }

    ui->toggle_shader_jit->setVisible(false);

    ConfigurationShared::SetColoredComboBox(
        ui->graphics_api_combo, ui->graphics_api_group,
        static_cast<u32>(Settings::values.graphics_api.GetValue(true)));

    ConfigurationShared::SetColoredTristate(ui->toggle_hw_shader, Settings::values.use_hw_shader,
                                            use_hw_shader);
    ConfigurationShared::SetColoredTristate(
        ui->toggle_accurate_mul, Settings::values.shaders_accurate_mul, shaders_accurate_mul);
    ConfigurationShared::SetColoredTristate(ui->toggle_disk_shader_cache,
                                            Settings::values.use_disk_shader_cache,
                                            use_disk_shader_cache);
    ConfigurationShared::SetColoredTristate(ui->toggle_vsync_new, Settings::values.use_vsync_new,
                                            use_vsync_new);
}
