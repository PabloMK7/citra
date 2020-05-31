// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <memory>
#include <utility>
#include <QInputDialog>
#include <QMenu>
#include <QMessageBox>
#include <QTimer>
#include "citra_qt/configuration/config.h"
#include "citra_qt/configuration/configure_input.h"
#include "citra_qt/configuration/configure_motion_touch.h"
#include "common/param_package.h"

const std::array<std::string, ConfigureInput::ANALOG_SUB_BUTTONS_NUM>
    ConfigureInput::analog_sub_buttons{{
        "up",
        "down",
        "left",
        "right",
        "modifier",
    }};

static QString GetKeyName(int key_code) {
    switch (key_code) {
    case Qt::Key_Shift:
        return QObject::tr("Shift");
    case Qt::Key_Control:
        return QObject::tr("Ctrl");
    case Qt::Key_Alt:
        return QObject::tr("Alt");
    case Qt::Key_Meta:
        return QString{};
    default:
        return QKeySequence(key_code).toString();
    }
}

static void SetAnalogButton(const Common::ParamPackage& input_param,
                            Common::ParamPackage& analog_param, const std::string& button_name) {
    if (analog_param.Get("engine", "") != "analog_from_button") {
        analog_param = {
            {"engine", "analog_from_button"},
        };
    }
    analog_param.Set(button_name, input_param.Serialize());
}

static QString ButtonToText(const Common::ParamPackage& param) {
    if (!param.Has("engine")) {
        return QObject::tr("[not set]");
    }

    if (param.Get("engine", "") == "keyboard") {
        return GetKeyName(param.Get("code", 0));
    }

    if (param.Get("engine", "") == "sdl") {
        if (param.Has("hat")) {
            const QString hat_str = QString::fromStdString(param.Get("hat", ""));
            const QString direction_str = QString::fromStdString(param.Get("direction", ""));

            return QObject::tr("Hat %1 %2").arg(hat_str, direction_str);
        }

        if (param.Has("axis")) {
            const QString axis_str = QString::fromStdString(param.Get("axis", ""));
            const QString direction_str = QString::fromStdString(param.Get("direction", ""));

            return QObject::tr("Axis %1%2").arg(axis_str, direction_str);
        }

        if (param.Has("button")) {
            const QString button_str = QString::fromStdString(param.Get("button", ""));

            return QObject::tr("Button %1").arg(button_str);
        }

        return {};
    }

    return QObject::tr("[unknown]");
}

static QString AnalogToText(const Common::ParamPackage& param, const std::string& dir) {
    if (!param.Has("engine")) {
        return QObject::tr("[not set]");
    }

    if (param.Get("engine", "") == "analog_from_button") {
        return ButtonToText(Common::ParamPackage{param.Get(dir, "")});
    }

    if (param.Get("engine", "") == "sdl") {
        if (dir == "modifier") {
            return QObject::tr("[unused]");
        }

        if (dir == "left" || dir == "right") {
            const QString axis_x_str = QString::fromStdString(param.Get("axis_x", ""));

            return QObject::tr("Axis %1").arg(axis_x_str);
        }

        if (dir == "up" || dir == "down") {
            const QString axis_y_str = QString::fromStdString(param.Get("axis_y", ""));

            return QObject::tr("Axis %1").arg(axis_y_str);
        }

        return {};
    }

    return QObject::tr("[unknown]");
}

ConfigureInput::ConfigureInput(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureInput>()),
      timeout_timer(std::make_unique<QTimer>()), poll_timer(std::make_unique<QTimer>()) {
    ui->setupUi(this);
    setFocusPolicy(Qt::ClickFocus);

    for (const auto& profile : Settings::values.input_profiles) {
        ui->profile->addItem(QString::fromStdString(profile.name));
    }

    ui->profile->setCurrentIndex(Settings::values.current_input_profile_index);

    button_map = {
        ui->buttonA,      ui->buttonB,        ui->buttonX,        ui->buttonY,
        ui->buttonDpadUp, ui->buttonDpadDown, ui->buttonDpadLeft, ui->buttonDpadRight,
        ui->buttonL,      ui->buttonR,        ui->buttonStart,    ui->buttonSelect,
        ui->buttonDebug,  ui->buttonGpio14,   ui->buttonZL,       ui->buttonZR,
        ui->buttonHome,
    };

    analog_map_buttons = {{
        {
            ui->buttonCircleUp,
            ui->buttonCircleDown,
            ui->buttonCircleLeft,
            ui->buttonCircleRight,
            ui->buttonCircleMod,
        },
        {
            ui->buttonCStickUp,
            ui->buttonCStickDown,
            ui->buttonCStickLeft,
            ui->buttonCStickRight,
            nullptr,
        },
    }};

    analog_map_stick = {ui->buttonCircleAnalog, ui->buttonCStickAnalog};
    analog_map_deadzone_and_modifier_slider = {ui->sliderCirclePadDeadzoneAndModifier,
                                               ui->sliderCStickDeadzoneAndModifier};
    analog_map_deadzone_and_modifier_slider_label = {ui->labelCirclePadDeadzoneAndModifier,
                                                     ui->labelCStickDeadzoneAndModifier};

    for (int button_id = 0; button_id < Settings::NativeButton::NumButtons; button_id++) {
        if (!button_map[button_id])
            continue;
        button_map[button_id]->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button_map[button_id], &QPushButton::clicked, [=]() {
            HandleClick(button_map[button_id],
                        [=](const Common::ParamPackage& params) {
                            buttons_param[button_id] = params;
                            // If the user closes the dialog, the changes are reverted in
                            // `GMainWindow::OnConfigure()`
                            ApplyConfiguration();
                            Settings::SaveProfile(ui->profile->currentIndex());
                        },
                        InputCommon::Polling::DeviceType::Button);
        });
        connect(button_map[button_id], &QPushButton::customContextMenuRequested,
                [=](const QPoint& menu_location) {
                    QMenu context_menu;
                    context_menu.addAction(tr("Clear"), [&] {
                        buttons_param[button_id].Clear();
                        button_map[button_id]->setText(tr("[not set]"));
                        ApplyConfiguration();
                        Settings::SaveProfile(ui->profile->currentIndex());
                    });
                    context_menu.addAction(tr("Restore Default"), [&] {
                        buttons_param[button_id] = Common::ParamPackage{
                            InputCommon::GenerateKeyboardParam(Config::default_buttons[button_id])};
                        button_map[button_id]->setText(ButtonToText(buttons_param[button_id]));
                        ApplyConfiguration();
                        Settings::SaveProfile(ui->profile->currentIndex());
                    });
                    context_menu.exec(button_map[button_id]->mapToGlobal(menu_location));
                });
    }

    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++) {
            if (!analog_map_buttons[analog_id][sub_button_id])
                continue;
            analog_map_buttons[analog_id][sub_button_id]->setContextMenuPolicy(
                Qt::CustomContextMenu);
            connect(analog_map_buttons[analog_id][sub_button_id], &QPushButton::clicked, [=]() {
                HandleClick(analog_map_buttons[analog_id][sub_button_id],
                            [=](const Common::ParamPackage& params) {
                                SetAnalogButton(params, analogs_param[analog_id],
                                                analog_sub_buttons[sub_button_id]);
                                ApplyConfiguration();
                                Settings::SaveProfile(ui->profile->currentIndex());
                            },
                            InputCommon::Polling::DeviceType::Button);
            });
            connect(analog_map_buttons[analog_id][sub_button_id],
                    &QPushButton::customContextMenuRequested, [=](const QPoint& menu_location) {
                        QMenu context_menu;
                        context_menu.addAction(tr("Clear"), [&] {
                            analogs_param[analog_id].Erase(analog_sub_buttons[sub_button_id]);
                            analog_map_buttons[analog_id][sub_button_id]->setText(tr("[not set]"));
                            ApplyConfiguration();
                            Settings::SaveProfile(ui->profile->currentIndex());
                        });
                        context_menu.addAction(tr("Restore Default"), [&] {
                            Common::ParamPackage params{InputCommon::GenerateKeyboardParam(
                                Config::default_analogs[analog_id][sub_button_id])};
                            SetAnalogButton(params, analogs_param[analog_id],
                                            analog_sub_buttons[sub_button_id]);
                            analog_map_buttons[analog_id][sub_button_id]->setText(AnalogToText(
                                analogs_param[analog_id], analog_sub_buttons[sub_button_id]));
                            ApplyConfiguration();
                            Settings::SaveProfile(ui->profile->currentIndex());
                        });
                        context_menu.exec(analog_map_buttons[analog_id][sub_button_id]->mapToGlobal(
                            menu_location));
                    });
        }
        connect(analog_map_stick[analog_id], &QPushButton::clicked, [=]() {
            if (QMessageBox::information(
                    this, tr("Information"),
                    tr("After pressing OK, first move your joystick horizontally, "
                       "and then vertically."),
                    QMessageBox::Ok | QMessageBox::Cancel) == QMessageBox::Ok) {
                HandleClick(analog_map_stick[analog_id],
                            [=](const Common::ParamPackage& params) {
                                analogs_param[analog_id] = params;
                                ApplyConfiguration();
                                Settings::SaveProfile(ui->profile->currentIndex());
                            },
                            InputCommon::Polling::DeviceType::Analog);
            }
        });
        connect(analog_map_deadzone_and_modifier_slider[analog_id], &QSlider::valueChanged, [=] {
            const float slider_value = analog_map_deadzone_and_modifier_slider[analog_id]->value();
            if (analogs_param[analog_id].Get("engine", "") == "sdl") {
                analog_map_deadzone_and_modifier_slider_label[analog_id]->setText(
                    tr("Deadzone: %1%").arg(slider_value));
                analogs_param[analog_id].Set("deadzone", slider_value / 100.0f);
            } else {
                analog_map_deadzone_and_modifier_slider_label[analog_id]->setText(
                    tr("Modifier Scale: %1%").arg(slider_value));
                analogs_param[analog_id].Set("modifier_scale", slider_value / 100.0f);
            }
        });
    }

    connect(ui->buttonMotionTouch, &QPushButton::clicked, [this] {
        QDialog* motion_touch_dialog = new ConfigureMotionTouch(this);
        return motion_touch_dialog->exec();
    });

    ui->buttonDelete->setEnabled(ui->profile->count() > 1);

    connect(ui->buttonClearAll, &QPushButton::clicked, this, &ConfigureInput::ClearAll);
    connect(ui->buttonRestoreDefaults, &QPushButton::clicked, this,
            &ConfigureInput::RestoreDefaults);
    connect(ui->buttonNew, &QPushButton::clicked, this, &ConfigureInput::NewProfile);
    connect(ui->buttonDelete, &QPushButton::clicked, this, &ConfigureInput::DeleteProfile);
    connect(ui->buttonRename, &QPushButton::clicked, this, &ConfigureInput::RenameProfile);

    connect(ui->profile, static_cast<void (QComboBox::*)(int)>(&QComboBox::currentIndexChanged),
            [this](int i) {
                ApplyConfiguration();
                Settings::SaveProfile(Settings::values.current_input_profile_index);
                Settings::LoadProfile(i);
                LoadConfiguration();
            });

    timeout_timer->setSingleShot(true);
    connect(timeout_timer.get(), &QTimer::timeout, [this]() { SetPollingResult({}, true); });

    connect(poll_timer.get(), &QTimer::timeout, [this]() {
        Common::ParamPackage params;
        for (auto& poller : device_pollers) {
            params = poller->GetNextInput();
            if (params.Has("engine")) {
                SetPollingResult(params, false);
                return;
            }
        }
    });

    LoadConfiguration();

    // TODO(wwylele): enable this when we actually emulate it
    ui->buttonHome->setEnabled(false);
}

ConfigureInput::~ConfigureInput() = default;

void ConfigureInput::ApplyConfiguration() {
    std::transform(buttons_param.begin(), buttons_param.end(),
                   Settings::values.current_input_profile.buttons.begin(),
                   [](const Common::ParamPackage& param) { return param.Serialize(); });
    std::transform(analogs_param.begin(), analogs_param.end(),
                   Settings::values.current_input_profile.analogs.begin(),
                   [](const Common::ParamPackage& param) { return param.Serialize(); });
}

void ConfigureInput::ApplyProfile() {
    Settings::values.current_input_profile_index = ui->profile->currentIndex();
}

void ConfigureInput::EmitInputKeysChanged() {
    emit InputKeysChanged(GetUsedKeyboardKeys());
}

void ConfigureInput::OnHotkeysChanged(QList<QKeySequence> new_key_list) {
    hotkey_list = new_key_list;
}

QList<QKeySequence> ConfigureInput::GetUsedKeyboardKeys() {
    QList<QKeySequence> list;
    for (int button = 0; button < Settings::NativeButton::NumButtons; button++) {
        // TODO(adityaruplaha): Add home button to list when we finally emulate it
        if (button == Settings::NativeButton::Home) {
            continue;
        }

        auto button_param = buttons_param[button];
        if (button_param.Get("engine", "") == "keyboard") {
            list << QKeySequence(button_param.Get("code", 0));
        }
    }
    return list;
}

void ConfigureInput::LoadConfiguration() {
    std::transform(Settings::values.current_input_profile.buttons.begin(),
                   Settings::values.current_input_profile.buttons.end(), buttons_param.begin(),
                   [](const std::string& str) { return Common::ParamPackage(str); });
    std::transform(Settings::values.current_input_profile.analogs.begin(),
                   Settings::values.current_input_profile.analogs.end(), analogs_param.begin(),
                   [](const std::string& str) { return Common::ParamPackage(str); });
    UpdateButtonLabels();
}

void ConfigureInput::RestoreDefaults() {
    for (int button_id = 0; button_id < Settings::NativeButton::NumButtons; button_id++) {
        buttons_param[button_id] = Common::ParamPackage{
            InputCommon::GenerateKeyboardParam(Config::default_buttons[button_id])};
    }

    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++) {
            Common::ParamPackage params{InputCommon::GenerateKeyboardParam(
                Config::default_analogs[analog_id][sub_button_id])};
            SetAnalogButton(params, analogs_param[analog_id], analog_sub_buttons[sub_button_id]);
        }
    }
    UpdateButtonLabels();

    ApplyConfiguration();
    Settings::SaveProfile(Settings::values.current_input_profile_index);
}

void ConfigureInput::ClearAll() {
    for (int button_id = 0; button_id < Settings::NativeButton::NumButtons; button_id++) {
        if (button_map[button_id] && button_map[button_id]->isEnabled())
            buttons_param[button_id].Clear();
    }
    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        analogs_param[analog_id].Clear();
    }
    UpdateButtonLabels();

    ApplyConfiguration();
    Settings::SaveProfile(Settings::values.current_input_profile_index);
}

void ConfigureInput::UpdateButtonLabels() {
    for (int button = 0; button < Settings::NativeButton::NumButtons; button++) {
        if (button_map[button])
            button_map[button]->setText(ButtonToText(buttons_param[button]));
    }

    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++) {
            if (analog_map_buttons[analog_id][sub_button_id]) {
                analog_map_buttons[analog_id][sub_button_id]->setText(
                    AnalogToText(analogs_param[analog_id], analog_sub_buttons[sub_button_id]));
            }
        }
        analog_map_stick[analog_id]->setText(tr("Set Analog Stick"));

        auto& param = analogs_param[analog_id];
        auto* const analog_stick_slider = analog_map_deadzone_and_modifier_slider[analog_id];
        auto* const analog_stick_slider_label =
            analog_map_deadzone_and_modifier_slider_label[analog_id];

        if (param.Has("engine")) {
            if (param.Get("engine", "") == "sdl") {
                if (!param.Has("deadzone")) {
                    param.Set("deadzone", 0.1f);
                }

                analog_stick_slider->setValue(static_cast<int>(param.Get("deadzone", 0.1f) * 100));
                if (analog_stick_slider->value() == 0) {
                    analog_stick_slider_label->setText(tr("Deadzone: 0%"));
                }
            } else {
                if (!param.Has("modifier_scale")) {
                    param.Set("modifier_scale", 0.5f);
                }

                analog_stick_slider->setValue(
                    static_cast<int>(param.Get("modifier_scale", 0.5f) * 100));
                if (analog_stick_slider->value() == 0) {
                    analog_stick_slider_label->setText(tr("Modifier Scale: 0%"));
                }
            }
        }
    }

    EmitInputKeysChanged();
}

void ConfigureInput::HandleClick(QPushButton* button,
                                 std::function<void(const Common::ParamPackage&)> new_input_setter,
                                 InputCommon::Polling::DeviceType type) {
    previous_key_code = QKeySequence(button->text())[0];
    button->setText(tr("[press key]"));
    button->setFocus();

    input_setter = new_input_setter;

    device_pollers = InputCommon::Polling::GetPollers(type);

    // Keyboard keys can only be used as button devices
    want_keyboard_keys = type == InputCommon::Polling::DeviceType::Button;

    for (auto& poller : device_pollers) {
        poller->Start();
    }

    grabKeyboard();
    grabMouse();
    timeout_timer->start(5000); // Cancel after 5 seconds
    poll_timer->start(200);     // Check for new inputs every 200ms
}

void ConfigureInput::SetPollingResult(const Common::ParamPackage& params, bool abort) {
    releaseKeyboard();
    releaseMouse();
    timeout_timer->stop();
    poll_timer->stop();
    for (auto& poller : device_pollers) {
        poller->Stop();
    }

    if (!abort && input_setter) {
        (*input_setter)(params);
    }

    UpdateButtonLabels();
    input_setter.reset();
}

void ConfigureInput::keyPressEvent(QKeyEvent* event) {
    if (!input_setter || !event)
        return;

    if (event->key() != Qt::Key_Escape && event->key() != previous_key_code) {
        if (want_keyboard_keys) {
            // Check if key is already bound
            if (hotkey_list.contains(QKeySequence(event->key())) ||
                GetUsedKeyboardKeys().contains(QKeySequence(event->key()))) {
                SetPollingResult({}, true);
                QMessageBox::critical(this, tr("Error!"),
                                      tr("You're using a key that's already bound."));
                return;
            }
            SetPollingResult(Common::ParamPackage{InputCommon::GenerateKeyboardParam(event->key())},
                             false);
        } else {
            // Escape key wasn't pressed and we don't want any keyboard keys, so don't stop
            // polling
            return;
        }
    }
    SetPollingResult({}, true);
    previous_key_code = 0;
}

void ConfigureInput::RetranslateUI() {
    ui->retranslateUi(this);
}

void ConfigureInput::NewProfile() {
    const QString name =
        QInputDialog::getText(this, tr("New Profile"), tr("Enter the name for the new profile."));
    if (name.isEmpty()) {
        return;
    }
    if (IsProfileNameDuplicate(name)) {
        WarnProposedProfileNameIsDuplicate();
        return;
    }

    ApplyConfiguration();
    Settings::SaveProfile(ui->profile->currentIndex());
    Settings::CreateProfile(name.toStdString());
    ui->profile->addItem(name);
    ui->profile->setCurrentIndex(Settings::values.current_input_profile_index);
    LoadConfiguration();
    ui->buttonDelete->setEnabled(ui->profile->count() > 1);
}

void ConfigureInput::DeleteProfile() {
    const auto answer = QMessageBox::question(
        this, tr("Delete Profile"), tr("Delete profile %1?").arg(ui->profile->currentText()));
    if (answer != QMessageBox::Yes) {
        return;
    }
    const int index = ui->profile->currentIndex();
    ui->profile->removeItem(index);
    ui->profile->setCurrentIndex(0);
    Settings::DeleteProfile(index);
    LoadConfiguration();
    ui->buttonDelete->setEnabled(ui->profile->count() > 1);
}

void ConfigureInput::RenameProfile() {
    const QString new_name = QInputDialog::getText(this, tr("Rename Profile"), tr("New name:"));
    if (new_name.isEmpty()) {
        return;
    }
    if (IsProfileNameDuplicate(new_name)) {
        WarnProposedProfileNameIsDuplicate();
        return;
    }

    ui->profile->setItemText(ui->profile->currentIndex(), new_name);
    Settings::RenameCurrentProfile(new_name.toStdString());
    Settings::SaveProfile(ui->profile->currentIndex());
}

bool ConfigureInput::IsProfileNameDuplicate(const QString& name) const {
    return ui->profile->findText(name, Qt::MatchFixedString | Qt::MatchCaseSensitive) != -1;
}

void ConfigureInput::WarnProposedProfileNameIsDuplicate() {
    QMessageBox::warning(this, tr("Duplicate profile name"),
                         tr("Profile name already exists. Please choose a different name."));
}
