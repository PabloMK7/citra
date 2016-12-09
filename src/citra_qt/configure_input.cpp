// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <utility>
#include <QTimer>
#include "citra_qt/config.h"
#include "citra_qt/configure_input.h"

static QString getKeyName(Qt::Key key_code) {
    switch (key_code) {
    case Qt::Key_Shift:
        return QObject::tr("Shift");
    case Qt::Key_Control:
        return QObject::tr("Ctrl");
    case Qt::Key_Alt:
        return QObject::tr("Alt");
    case Qt::Key_Meta:
    case -1:
        return "";
    default:
        return QKeySequence(key_code).toString();
    }
}

ConfigureInput::ConfigureInput(QWidget* parent)
    : QWidget(parent), ui(std::make_unique<Ui::ConfigureInput>()),
      timer(std::make_unique<QTimer>()) {

    ui->setupUi(this);
    setFocusPolicy(Qt::ClickFocus);

    button_map = {
        {Settings::NativeInput::Values::A, ui->buttonA},
        {Settings::NativeInput::Values::B, ui->buttonB},
        {Settings::NativeInput::Values::X, ui->buttonX},
        {Settings::NativeInput::Values::Y, ui->buttonY},
        {Settings::NativeInput::Values::L, ui->buttonL},
        {Settings::NativeInput::Values::R, ui->buttonR},
        {Settings::NativeInput::Values::ZL, ui->buttonZL},
        {Settings::NativeInput::Values::ZR, ui->buttonZR},
        {Settings::NativeInput::Values::START, ui->buttonStart},
        {Settings::NativeInput::Values::SELECT, ui->buttonSelect},
        {Settings::NativeInput::Values::HOME, ui->buttonHome},
        {Settings::NativeInput::Values::DUP, ui->buttonDpadUp},
        {Settings::NativeInput::Values::DDOWN, ui->buttonDpadDown},
        {Settings::NativeInput::Values::DLEFT, ui->buttonDpadLeft},
        {Settings::NativeInput::Values::DRIGHT, ui->buttonDpadRight},
        {Settings::NativeInput::Values::CUP, ui->buttonCStickUp},
        {Settings::NativeInput::Values::CDOWN, ui->buttonCStickDown},
        {Settings::NativeInput::Values::CLEFT, ui->buttonCStickLeft},
        {Settings::NativeInput::Values::CRIGHT, ui->buttonCStickRight},
        {Settings::NativeInput::Values::CIRCLE_UP, ui->buttonCircleUp},
        {Settings::NativeInput::Values::CIRCLE_DOWN, ui->buttonCircleDown},
        {Settings::NativeInput::Values::CIRCLE_LEFT, ui->buttonCircleLeft},
        {Settings::NativeInput::Values::CIRCLE_RIGHT, ui->buttonCircleRight},
        {Settings::NativeInput::Values::CIRCLE_MODIFIER, ui->buttonCircleMod},
    };

    for (const auto& entry : button_map) {
        const Settings::NativeInput::Values input_id = entry.first;
        connect(entry.second, &QPushButton::released,
                [this, input_id]() { handleClick(input_id); });
    }

    connect(ui->buttonRestoreDefaults, &QPushButton::released, [this]() { restoreDefaults(); });

    timer->setSingleShot(true);
    connect(timer.get(), &QTimer::timeout, [this]() {
        releaseKeyboard();
        releaseMouse();
        current_input_id = boost::none;
        updateButtonLabels();
    });

    this->loadConfiguration();
}

void ConfigureInput::applyConfiguration() {
    for (const auto& input_id : Settings::NativeInput::All) {
        const size_t index = static_cast<size_t>(input_id);
        Settings::values.input_mappings[index] = static_cast<int>(key_map[input_id]);
    }
    Settings::Apply();
}

void ConfigureInput::loadConfiguration() {
    for (const auto& input_id : Settings::NativeInput::All) {
        const size_t index = static_cast<size_t>(input_id);
        key_map[input_id] = static_cast<Qt::Key>(Settings::values.input_mappings[index]);
    }
    updateButtonLabels();
}

void ConfigureInput::restoreDefaults() {
    for (const auto& input_id : Settings::NativeInput::All) {
        const size_t index = static_cast<size_t>(input_id);
        key_map[input_id] = static_cast<Qt::Key>(Config::defaults[index].toInt());
    }
    updateButtonLabels();
    applyConfiguration();
}

void ConfigureInput::updateButtonLabels() {
    for (const auto& input_id : Settings::NativeInput::All) {
        button_map[input_id]->setText(getKeyName(key_map[input_id]));
    }
}

void ConfigureInput::handleClick(Settings::NativeInput::Values input_id) {
    QPushButton* button = button_map[input_id];
    button->setText(tr("[press key]"));
    button->setFocus();

    current_input_id = input_id;

    grabKeyboard();
    grabMouse();
    timer->start(5000); // Cancel after 5 seconds
}

void ConfigureInput::keyPressEvent(QKeyEvent* event) {
    releaseKeyboard();
    releaseMouse();

    if (!current_input_id || !event)
        return;

    if (event->key() != Qt::Key_Escape)
        setInput(*current_input_id, static_cast<Qt::Key>(event->key()));

    updateButtonLabels();
    current_input_id = boost::none;
    timer->stop();
}

void ConfigureInput::setInput(Settings::NativeInput::Values input_id, Qt::Key key_pressed) {
    // Remove duplicates
    for (auto& pair : key_map) {
        if (pair.second == key_pressed)
            pair.second = Qt::Key_unknown;
    }

    key_map[input_id] = key_pressed;
}
