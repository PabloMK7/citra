// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <functional>
#include <memory>
#include <string>
#include <QKeyEvent>
#include <QWidget>
#include <boost/optional.hpp>
#include "common/param_package.h"
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

    std::unique_ptr<QTimer> timer;

    /// This will be the the setting function when an input is awaiting configuration.
    boost::optional<std::function<void(int)>> key_setter;

    std::array<Common::ParamPackage, Settings::NativeButton::NumButtons> buttons_param;
    std::array<Common::ParamPackage, Settings::NativeAnalog::NumAnalogs> analogs_param;

    static constexpr int ANALOG_SUB_BUTTONS_NUM = 5;

    /// Each button input is represented by a QPushButton.
    std::array<QPushButton*, Settings::NativeButton::NumButtons> button_map;

    /// Each analog input is represented by five QPushButtons which represents up, down, left, right
    /// and modifier
    std::array<std::array<QPushButton*, ANALOG_SUB_BUTTONS_NUM>, Settings::NativeAnalog::NumAnalogs>
        analog_map;

    static const std::array<std::string, ANALOG_SUB_BUTTONS_NUM> analog_sub_buttons;

    /// Load configuration settings.
    void loadConfiguration();
    /// Restore all buttons to their default values.
    void restoreDefaults();
    /// Update UI to reflect current configuration.
    void updateButtonLabels();

    /// Called when the button was pressed.
    void handleClick(QPushButton* button, std::function<void(int)> new_key_setter);
    /// Handle key press events.
    void keyPressEvent(QKeyEvent* event) override;
};
