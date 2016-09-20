// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDockWidget>
#include "ui_registers.h"

class QTreeWidget;
class QTreeWidgetItem;
class EmuThread;

class RegistersWidget : public QDockWidget {
    Q_OBJECT

public:
    RegistersWidget(QWidget* parent = nullptr);

public slots:
    void OnDebugModeEntered();
    void OnDebugModeLeft();

    void OnEmulationStarting(EmuThread* emu_thread);
    void OnEmulationStopping();

private:
    void CreateCPSRChildren();
    void UpdateCPSRValues();

    void CreateVFPSystemRegisterChildren();
    void UpdateVFPSystemRegisterValues();

    Ui::ARMRegisters cpu_regs_ui;

    QTreeWidget* tree;

    QTreeWidgetItem* core_registers;
    QTreeWidgetItem* vfp_registers;
    QTreeWidgetItem* vfp_system_registers;
    QTreeWidgetItem* cpsr;
};
