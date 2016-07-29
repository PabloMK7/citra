// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>
#include <utility>
#include <QTimer>

#include "citra_qt/configure_input.h"

ConfigureInput::ConfigureInput(QWidget* parent) : QWidget(parent), ui(std::make_unique<Ui::ConfigureInput>()) {
    ui->setupUi(this);

    // Initialize mapping of input enum to UI button.
    input_mapping = {
        { std::make_pair(Settings::NativeInput::Values::A, ui->buttonA) },
        { std::make_pair(Settings::NativeInput::Values::B, ui->buttonB) },
        { std::make_pair(Settings::NativeInput::Values::X, ui->buttonX) },
        { std::make_pair(Settings::NativeInput::Values::Y, ui->buttonY) },
        { std::make_pair(Settings::NativeInput::Values::L, ui->buttonL) },
        { std::make_pair(Settings::NativeInput::Values::R, ui->buttonR) },
        { std::make_pair(Settings::NativeInput::Values::ZL, ui->buttonZL) },
        { std::make_pair(Settings::NativeInput::Values::ZR, ui->buttonZR) },
        { std::make_pair(Settings::NativeInput::Values::START, ui->buttonStart) },
        { std::make_pair(Settings::NativeInput::Values::SELECT, ui->buttonSelect) },
        { std::make_pair(Settings::NativeInput::Values::HOME, ui->buttonHome) },
        { std::make_pair(Settings::NativeInput::Values::DUP, ui->buttonDpadUp) },
        { std::make_pair(Settings::NativeInput::Values::DDOWN, ui->buttonDpadDown) },
        { std::make_pair(Settings::NativeInput::Values::DLEFT, ui->buttonDpadLeft) },
        { std::make_pair(Settings::NativeInput::Values::DRIGHT, ui->buttonDpadRight) },
        { std::make_pair(Settings::NativeInput::Values::CUP, ui->buttonCStickUp) },
        { std::make_pair(Settings::NativeInput::Values::CDOWN, ui->buttonCStickDown) },
        { std::make_pair(Settings::NativeInput::Values::CLEFT, ui->buttonCStickLeft) },
        { std::make_pair(Settings::NativeInput::Values::CRIGHT, ui->buttonCStickRight) },
        { std::make_pair(Settings::NativeInput::Values::CIRCLE_UP, ui->buttonCircleUp) },
        { std::make_pair(Settings::NativeInput::Values::CIRCLE_DOWN, ui->buttonCircleDown) },
        { std::make_pair(Settings::NativeInput::Values::CIRCLE_LEFT, ui->buttonCircleLeft) },
        { std::make_pair(Settings::NativeInput::Values::CIRCLE_RIGHT, ui->buttonCircleRight) },
        { std::make_pair(Settings::NativeInput::Values::CIRCLE_MODIFIER, ui->buttonCircleMod) },
    };

    // Attach handle click method to each button click.
    for (const auto& entry : input_mapping) {
        connect(entry.second, SIGNAL(released()), this, SLOT(handleClick()));
    }
    connect(ui->buttonRestoreDefaults, SIGNAL(released()), this, SLOT(restoreDefaults()));
    setFocusPolicy(Qt::ClickFocus);
    timer = new QTimer(this);
    timer->setSingleShot(true);
    connect(timer, &QTimer::timeout, this, [&]() { key_pressed = Qt::Key_Escape; setKey(); });
    this->setConfiguration();
}

void ConfigureInput::handleClick() {
    QPushButton* sender = qobject_cast<QPushButton*>(QObject::sender());
    previous_mapping = sender->text();
    sender->setText(tr("[waiting]"));
    sender->setFocus();
    grabKeyboard();
    grabMouse();
    changing_button = sender;
    timer->start(5000); //Cancel after 5 seconds
}

void ConfigureInput::applyConfiguration() {
    for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
        int value = getKeyValue(input_mapping[Settings::NativeInput::Values(i)]->text());
        Settings::values.input_mappings[Settings::NativeInput::All[i]] = value;
    }
    Settings::Apply();
}

void ConfigureInput::setConfiguration() {
    for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
        QString keyValue = getKeyName(Settings::values.input_mappings[i]);
        input_mapping[Settings::NativeInput::Values(i)]->setText(keyValue);
    }
}

void ConfigureInput::keyPressEvent(QKeyEvent* event) {
    if (!changing_button)
        return;
    if (!event || event->key() == Qt::Key_unknown)
        return;
    key_pressed = event->key();
    timer->stop();
    setKey();
}

void ConfigureInput::setKey() {
    const QString key_value = getKeyName(key_pressed);
    if (key_pressed == Qt::Key_Escape)
        changing_button->setText(previous_mapping);
    else
        changing_button->setText(key_value);
    removeDuplicates(key_value);
    key_pressed = Qt::Key_unknown;
    releaseKeyboard();
    releaseMouse();
    changing_button = nullptr;
    previous_mapping = nullptr;
}

QString ConfigureInput::getKeyName(int key_code) const {
    if (key_code == Qt::Key_Shift)
        return tr("Shift");
    if (key_code == Qt::Key_Control)
        return tr("Ctrl");
    if (key_code == Qt::Key_Alt)
        return tr("Alt");
    if (key_code == Qt::Key_Meta)
        return "";
    if (key_code == -1)
        return "";

    return QKeySequence(key_code).toString();
}

Qt::Key ConfigureInput::getKeyValue(const QString& text) const {
    if (text == "Shift")
        return Qt::Key_Shift;
    if (text == "Ctrl")
        return Qt::Key_Control;
    if (text == "Alt")
        return Qt::Key_Alt;
    if (text == "Meta")
        return Qt::Key_unknown;
    if (text == "")
        return Qt::Key_unknown;

    return Qt::Key(QKeySequence(text)[0]);
}

void ConfigureInput::removeDuplicates(const QString& newValue) {
    for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
        if (changing_button != input_mapping[Settings::NativeInput::Values(i)]) {
            const QString oldValue = input_mapping[Settings::NativeInput::Values(i)]->text();
            if (newValue == oldValue)
                input_mapping[Settings::NativeInput::Values(i)]->setText("");
        }
    }
}

void ConfigureInput::restoreDefaults() {
    for (int i = 0; i < Settings::NativeInput::NUM_INPUTS; ++i) {
        const QString keyValue = getKeyName(Config::defaults[i].toInt());
        input_mapping[Settings::NativeInput::Values(i)]->setText(keyValue);
    }
}
