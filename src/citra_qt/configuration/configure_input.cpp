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
        return "";
    default:
        return QKeySequence(key_code).toString();
    }
}

static void SetAnalogButton(const Common::ParamPackage& input_param,
                            Common::ParamPackage& analog_param, const std::string& button_name) {
    if (analog_param.Get("engine", "") != "analog_from_button") {
        analog_param = {
            {"engine", "analog_from_button"},
            {"modifier_scale", "0.5"},
        };
    }
    analog_param.Set(button_name, input_param.Serialize());
}

static QString ButtonToText(const Common::ParamPackage& param) {
    if (!param.Has("engine")) {
        return QObject::tr("[not set]");
    } else if (param.Get("engine", "") == "keyboard") {
        return GetKeyName(param.Get("code", 0));
    } else if (param.Get("engine", "") == "sdl") {
        if (param.Has("hat")) {
            return QString(QObject::tr("Hat %1 %2"))
                .arg(param.Get("hat", "").c_str(), param.Get("direction", "").c_str());
        }
        if (param.Has("axis")) {
            return QString(QObject::tr("Axis %1%2"))
                .arg(param.Get("axis", "").c_str(), param.Get("direction", "").c_str());
        }
        if (param.Has("button")) {
            return QString(QObject::tr("Button %1")).arg(param.Get("button", "").c_str());
        }
        return QString();
    } else {
        return QObject::tr("[unknown]");
    }
};

static QString AnalogToText(const Common::ParamPackage& param, const std::string& dir) {
    if (!param.Has("engine")) {
        return QObject::tr("[not set]");
    } else if (param.Get("engine", "") == "analog_from_button") {
        return ButtonToText(Common::ParamPackage{param.Get(dir, "")});
    } else if (param.Get("engine", "") == "sdl") {
        if (dir == "modifier") {
            return QString(QObject::tr("[unused]"));
        }

        if (dir == "left" || dir == "right") {
            return QString(QObject::tr("Axis %1")).arg(param.Get("axis_x", "").c_str());
        } else if (dir == "up" || dir == "down") {
            return QString(QObject::tr("Axis %1")).arg(param.Get("axis_y", "").c_str());
        }
        return QString();
    } else {
        return QObject::tr("[unknown]");
    }
};

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

    for (int button_id = 0; button_id < Settings::NativeButton::NumButtons; button_id++) {
        if (!button_map[button_id])
            continue;
        button_map[button_id]->setContextMenuPolicy(Qt::CustomContextMenu);
        connect(button_map[button_id], &QPushButton::released, [=]() {
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
            connect(analog_map_buttons[analog_id][sub_button_id], &QPushButton::released, [=]() {
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
        connect(analog_map_stick[analog_id], &QPushButton::released, [=]() {
            QMessageBox::information(this, tr("Information"),
                                     tr("After pressing OK, first move your joystick horizontally, "
                                        "and then vertically."));
            HandleClick(analog_map_stick[analog_id],
                        [=](const Common::ParamPackage& params) {
                            analogs_param[analog_id] = params;
                            ApplyConfiguration();
                            Settings::SaveProfile(ui->profile->currentIndex());
                        },
                        InputCommon::Polling::DeviceType::Analog);
        });
    }

    connect(ui->buttonMotionTouch, &QPushButton::released, [this] {
        QDialog* motion_touch_dialog = new ConfigureMotionTouch(this);
        return motion_touch_dialog->exec();
    });

    ui->buttonDelete->setEnabled(ui->profile->count() > 1);

    connect(ui->buttonClearAll, &QPushButton::released, [this] { ClearAll(); });
    connect(ui->buttonRestoreDefaults, &QPushButton::released, [this]() { RestoreDefaults(); });
    connect(ui->buttonNew, &QPushButton::released, [this] { NewProfile(); });
    connect(ui->buttonDelete, &QPushButton::released, [this] { DeleteProfile(); });
    connect(ui->buttonRename, &QPushButton::released, [this] { RenameProfile(); });

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
}

void ConfigureInput::ClearAll() {
    for (int button_id = 0; button_id < Settings::NativeButton::NumButtons; button_id++) {
        if (button_map[button_id] && button_map[button_id]->isEnabled())
            buttons_param[button_id].Clear();
    }
    for (int analog_id = 0; analog_id < Settings::NativeAnalog::NumAnalogs; analog_id++) {
        for (int sub_button_id = 0; sub_button_id < ANALOG_SUB_BUTTONS_NUM; sub_button_id++) {
            if (analog_map_buttons[analog_id][sub_button_id] &&
                analog_map_buttons[analog_id][sub_button_id]->isEnabled())
                analogs_param[analog_id].Erase(analog_sub_buttons[sub_button_id]);
        }
    }
    UpdateButtonLabels();
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
}

bool ConfigureInput::IsProfileNameDuplicate(const QString& name) const {
    return ui->profile->findText(name, Qt::MatchFixedString | Qt::MatchCaseSensitive) != -1;
}

void ConfigureInput::WarnProposedProfileNameIsDuplicate() {
    QMessageBox::warning(this, tr("Duplicate profile name"),
                         tr("Profile name already exists. Please choose a different name."));
}
