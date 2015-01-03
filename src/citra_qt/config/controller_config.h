// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifndef _CONTROLLER_CONFIG_HXX_
#define _CONTROLLER_CONFIG_HXX_

#include <QDialog>

//#include "ui_controller_config.h"

/* TODO(bunnei): ImplementMe

#include "config.h"

class GControllerConfig : public QWidget
{
    Q_OBJECT

public:
    GControllerConfig(common::Config::ControllerPort* initial_config, QWidget* parent = NULL);

    const common::Config::ControllerPort& GetControllerConfig(int index) const { return config[index]; }

signals:
    void ActivePortChanged(const common::Config::ControllerPort&);

public slots:
    void OnKeyConfigChanged(common::Config::Control id, int key, const QString& name);

private:
    int GetActiveController();
    bool InputSourceJoypad();

    Ui::ControllerConfig ui;
    common::Config::ControllerPort config[4];
};

class GControllerConfigDialog : public QDialog
{
    Q_OBJECT

public:
    GControllerConfigDialog(common::Config::ControllerPort* controller_ports, QWidget* parent = NULL);

public slots:
    void EnableChanges();

private:
    GControllerConfig* config_widget;
    common::Config::ControllerPort* config_ptr;
};

*/

#endif  // _CONTROLLER_CONFIG_HXX_
