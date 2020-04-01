// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QDialog>
#include "common/param_package.h"
#include "core/settings.h"
#include "input_common/udp/udp.h"

class QVBoxLayout;
class QLabel;
class QPushButton;

namespace Ui {
class ConfigureMotionTouch;
}

/// A dialog for touchpad calibration configuration.
class CalibrationConfigurationDialog : public QDialog {
    Q_OBJECT
public:
    explicit CalibrationConfigurationDialog(QWidget* parent, const std::string& host, u16 port,
                                            u8 pad_index, u16 client_id);
    ~CalibrationConfigurationDialog();

private:
    Q_INVOKABLE void UpdateLabelText(QString text);
    Q_INVOKABLE void UpdateButtonText(QString text);

    QVBoxLayout* layout;
    QLabel* status_label;
    QPushButton* cancel_button;
    std::unique_ptr<InputCommon::CemuhookUDP::CalibrationConfigurationJob> job;

    // Configuration results
    bool completed{};
    u16 min_x, min_y, max_x, max_y;

    friend class ConfigureMotionTouch;
};

class ConfigureMotionTouch : public QDialog {
    Q_OBJECT

public:
    explicit ConfigureMotionTouch(QWidget* parent = nullptr);
    ~ConfigureMotionTouch() override;

public slots:
    void ApplyConfiguration();

private slots:
    void OnCemuhookUDPTest();
    void OnConfigureTouchCalibration();
    void OnConfigureTouchFromButton();

private:
    void closeEvent(QCloseEvent* event) override;
    Q_INVOKABLE void ShowUDPTestResult(bool result);
    void SetConfiguration();
    void UpdateUiDisplay();
    void ConnectEvents();
    bool CanCloseDialog();

    std::unique_ptr<Ui::ConfigureMotionTouch> ui;

    // Coordinate system of the CemuhookUDP touch provider
    int min_x, min_y, max_x, max_y;

    bool udp_test_in_progress{};

    std::vector<Settings::TouchFromButtonMap> touch_from_button_maps;
};
