// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QKeyEvent>
#include <QWidget>
#include <boost/optional.hpp>
#include "core/settings.h"
#include "ui_configure_input.h"

class QPushButton;
class QString;
class QTimer;

namespace Ui {
class ConfigureInput;
}

class ConfigureInput : public QWidget {
    Q_OBJECT

public:
    explicit ConfigureInput(QWidget* parent = nullptr);

    /// Save all button configurations to settings file
    void applyConfiguration();

private:
    std::unique_ptr<Ui::ConfigureInput> ui;

    /// This input is currently awaiting configuration.
    /// (i.e.: its corresponding QPushButton has been pressed.)
    boost::optional<Settings::NativeInput::Values> current_input_id;
    std::unique_ptr<QTimer> timer;

    /// Each input is represented by a QPushButton.
    std::map<Settings::NativeInput::Values, QPushButton*> button_map;
    /// Each input is configured to respond to the press of a Qt::Key.
    std::map<Settings::NativeInput::Values, Qt::Key> key_map;

    /// Load configuration settings.
    void loadConfiguration();
    /// Restore all buttons to their default values.
    void restoreDefaults();
    /// Update UI to reflect current configuration.
    void updateButtonLabels();

    /// Called when the button corresponding to input_id was pressed.
    void handleClick(Settings::NativeInput::Values input_id);
    /// Handle key press events.
    void keyPressEvent(QKeyEvent* event) override;
    /// Configure input input_id to respond to key key_pressed.
    void setInput(Settings::NativeInput::Values input_id, Qt::Key key_pressed);
};
