// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <utility>
#include <QTimer>
#include "citra_qt/configuration/config.h"
#include "citra_qt/configuration/configure_input.h"
#include "common/param_package.h"
#include "input_common/main.h"

const std::array<std::string, ConfigureInput::ANALOG_SUB_BUTTONS_NUM>
    ConfigureInput::analog_sub_buttons{{
        "up", "down", "left", "right", "modifier",
    }};

static QString getKeyName(int key_code) {
    switch (key_code) {
    case Qt::Key_Shift:
        return QObject::tr("Shift");
    case Qt::Key_Control:
        return QObject::tr("Ctrl");
    case Qt::Key_Alt:
        return QObject::tr("Alt");
    case Qt::Key_Meta:
        return "";
    default:
        return QKeySequence(key_code).toString();
    }
}

static void SetButtonKey(int key, Common::ParamPackage& button_param) {
    button_param = Common::ParamPackage{InputCommon::GenerateKeyboardParam(key)};
}

static void SetAnalogKey(int key, Common::ParamPackage& analog_param,
                         const std::string& button_name) {
    if (analog_param.Get("engine", "") != "analog_from_button") {
        analog_param = {
            {"engine", "analog_from_button"}, {"modifier_scale", "0.5"},
        };
    }
    analog_param.Set(button_name, InputCommon::GenerateKeyboardParam(key));
}

ConfigureInput::ConfigureInput(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureInput>()),
      timer(std::make_unique<QTimer>()) {

    ui->setupUi(this);
    setFocusPolicy(Qt::ClickFocus);

    button_map = {
        ui->buttonA,        ui->buttonB,        ui->buttonX,         ui->buttonY,  ui->buttonDpadUp,
        ui->buttonDpadDown, ui->buttonDpadLeft, ui->buttonDpadRight, ui->buttonL,  ui->buttonR,
        ui->buttonStart,    ui->buttonSelect,   ui->buttonZL,        ui->buttonZR, ui->buttonHome,
    };

    analog_map = {{
        {
            ui->buttonCircleUp, ui->buttonCircleDown, ui->buttonCircleLeft, ui->buttonCircleRight,
            ui->buttonCircleMod,
        },
        {
            ui->buttonCStickUp, ui->buttonCStickDown, ui->buttonCStickLeft, ui->buttonCStickRight,
            nullptr,
        },
    }};

    for (int button_id = 0; button_id < Settings::NativeButton::NumButtons; button_id++) {
        if (button_map[button_id])
            connect(button_map[button_id], &QPushButton::released, [=]() {
                handleClick(button_map[button_id],
                            [=](int key) { SetButtonKey(key, buttons_param[button_id]); });
            });
    }

    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++) {
            if (analog_map[analog_id][sub_button_id] != nullptr) {
                connect(analog_map[analog_id][sub_button_id], &QPushButton::released, [=]() {
                    handleClick(analog_map[analog_id][sub_button_id], [=](int key) {
                        SetAnalogKey(key, analogs_param[analog_id],
                                     analog_sub_buttons[sub_button_id]);
                    });
                });
            }
        }
    }

    connect(ui->buttonRestoreDefaults, &QPushButton::released, [this]() { restoreDefaults(); });

    timer->setSingleShot(true);
    connect(timer.get(), &QTimer::timeout, [this]() {
        releaseKeyboard();
        releaseMouse();
        key_setter = boost::none;
        updateButtonLabels();
    });

    this->loadConfiguration();

    // TODO(wwylele): enable this when we actually emulate it
    ui->buttonHome->setEnabled(false);
}

void ConfigureInput::applyConfiguration() {
    std::transform(buttons_param.begin(), buttons_param.end(), Settings::values.buttons.begin(),
                   [](const Common::ParamPackage& param) { return param.Serialize(); });
    std::transform(analogs_param.begin(), analogs_param.end(), Settings::values.analogs.begin(),
                   [](const Common::ParamPackage& param) { return param.Serialize(); });

    Settings::Apply();
}

void ConfigureInput::loadConfiguration() {
    std::transform(Settings::values.buttons.begin(), Settings::values.buttons.end(),
                   buttons_param.begin(),
                   [](const std::string& str) { return Common::ParamPackage(str); });
    std::transform(Settings::values.analogs.begin(), Settings::values.analogs.end(),
                   analogs_param.begin(),
                   [](const std::string& str) { return Common::ParamPackage(str); });
    updateButtonLabels();
}

void ConfigureInput::restoreDefaults() {
    for (int button_id = 0; button_id < Settings::NativeButton::NumButtons; button_id++) {
        SetButtonKey(Config::default_buttons[button_id], buttons_param[button_id]);
    }

    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++) {
            SetAnalogKey(Config::default_analogs[analog_id][sub_button_id],
                         analogs_param[analog_id], analog_sub_buttons[sub_button_id]);
        }
    }
    updateButtonLabels();
    applyConfiguration();
}

void ConfigureInput::updateButtonLabels() {
    QString non_keyboard(tr("[non-keyboard]"));

    auto KeyToText = [&non_keyboard](const Common::ParamPackage& param) {
        if (param.Get("engine", "") != "keyboard") {
            return non_keyboard;
        } else {
            return getKeyName(param.Get("code", 0));
        }
    };

    for (int button = 0; button < Settings::NativeButton::NumButtons; button++) {
        button_map[button]->setText(KeyToText(buttons_param[button]));
    }

    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        if (analogs_param[analog_id].Get("engine", "") != "analog_from_button") {
            for (QPushButton* button : analog_map[analog_id]) {
                if (button)
                    button->setText(non_keyboard);
            }
        } else {
            for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++) {
                Common::ParamPackage param(
                    analogs_param[analog_id].Get(analog_sub_buttons[sub_button_id], ""));
                if (analog_map[analog_id][sub_button_id])
                    analog_map[analog_id][sub_button_id]->setText(KeyToText(param));
            }
        }
    }
}

void ConfigureInput::handleClick(QPushButton* button, std::function<void(int)> new_key_setter) {
    button->setText(tr("[press key]"));
    button->setFocus();

    key_setter = new_key_setter;

    grabKeyboard();
    grabMouse();
    timer->start(5000); // Cancel after 5 seconds
}

void ConfigureInput::keyPressEvent(QKeyEvent* event) {
    releaseKeyboard();
    releaseMouse();

    if (!key_setter || !event)
        return;

    if (event->key() != Qt::Key_Escape)
        (*key_setter)(event->key());

    updateButtonLabels();
    key_setter = boost::none;
    timer->stop();
}
