// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDialogButtonBox>

#include "controller_config.h"
#include "controller_config_util.h"

/* TODO(bunnei): ImplementMe

using common::Config;

GControllerConfig::GControllerConfig(common::Config::ControllerPort* initial_config, QWidget* parent) : QWidget(parent)
{
    ui.setupUi(this);
    ((QGridLayout*)ui.mainStickTab->layout())->addWidget(new GStickConfig(Config::ANALOG_LEFT, Config::ANALOG_RIGHT, Config::ANALOG_UP, Config::ANALOG_DOWN, this, this), 1, 1);
    ((QGridLayout*)ui.cStickTab->layout())->addWidget(new GStickConfig(Config::C_LEFT, Config::C_RIGHT, Config::C_UP, Config::C_DOWN, this, this), 1, 1);
    ((QGridLayout*)ui.dPadTab->layout())->addWidget(new GStickConfig(Config::DPAD_LEFT, Config::DPAD_RIGHT, Config::DPAD_UP, Config::DPAD_DOWN, this, this), 1, 1);

    // TODO: Arrange these more compactly?
    QVBoxLayout* layout = (QVBoxLayout*)ui.buttonsTab->layout();
    layout->addWidget(new GButtonConfigGroup("A Button", Config::BUTTON_A, this, ui.buttonsTab));
    layout->addWidget(new GButtonConfigGroup("B Button", Config::BUTTON_B, this, ui.buttonsTab));
    layout->addWidget(new GButtonConfigGroup("X Button", Config::BUTTON_X, this, ui.buttonsTab));
    layout->addWidget(new GButtonConfigGroup("Y Button", Config::BUTTON_Y, this, ui.buttonsTab));
    layout->addWidget(new GButtonConfigGroup("Z Button", Config::BUTTON_Z, this, ui.buttonsTab));
    layout->addWidget(new GButtonConfigGroup("L Trigger", Config::TRIGGER_L, this, ui.buttonsTab));
    layout->addWidget(new GButtonConfigGroup("R Trigger", Config::TRIGGER_R, this, ui.buttonsTab));
    layout->addWidget(new GButtonConfigGroup("Start Button", Config::BUTTON_START, this, ui.buttonsTab));

    memcpy(config, initial_config, sizeof(config));

    emit ActivePortChanged(config[0]);
}

void GControllerConfig::OnKeyConfigChanged(common::Config::Control id, int key, const QString& name)
{
    if (InputSourceJoypad())
    {
        config[GetActiveController()].pads.key_code[id] = key;
    }
    else
    {
        config[GetActiveController()].keys.key_code[id] = key;
    }
    emit ActivePortChanged(config[GetActiveController()]);
}

int GControllerConfig::GetActiveController()
{
    return ui.activeControllerCB->currentIndex();
}

bool GControllerConfig::InputSourceJoypad()
{
    return ui.inputSourceCB->currentIndex() == 1;
}

GControllerConfigDialog::GControllerConfigDialog(common::Config::ControllerPort* controller_ports, QWidget* parent) : QDialog(parent), config_ptr(controller_ports)
{
    setWindowTitle(tr("Input configuration"));

    QVBoxLayout* layout = new QVBoxLayout(this);
    config_widget = new GControllerConfig(controller_ports, this);
    layout->addWidget(config_widget);

    QDialogButtonBox* buttons = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    layout->addWidget(buttons);

    connect(buttons, SIGNAL(rejected()), this, SLOT(reject()));
    connect(buttons, SIGNAL(accepted()), this, SLOT(accept()));

    connect(this, SIGNAL(accepted()), this, SLOT(EnableChanges()));

    layout->setSizeConstraint(QLayout::SetFixedSize);
    setLayout(layout);
    setModal(true);
    show();
}

void GControllerConfigDialog::EnableChanges()
{
    for (unsigned int i = 0; i < 4; ++i)
    {
        memcpy(&config_ptr[i], &config_widget->GetControllerConfig(i), sizeof(common::Config::ControllerPort));

        if (common::g_config) {
            // Apply changes if running a game
            memcpy(&common::g_config->controller_ports(i), &config_widget->GetControllerConfig(i), sizeof(common::Config::ControllerPort));
        }
    }
}

*/
