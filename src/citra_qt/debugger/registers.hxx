// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "ui_registers.h"

#include <QDockWidget>
#include <QTreeWidgetItem>

class QTreeWidget;

class RegistersWidget : public QDockWidget
{
    Q_OBJECT

public:
    RegistersWidget(QWidget* parent = NULL);

public slots:
    void OnCPUStepped();

private:
    Ui::ARMRegisters cpu_regs_ui;

    QTreeWidget* tree;

    QTreeWidgetItem* registers;
    QTreeWidgetItem* CSPR;
};
