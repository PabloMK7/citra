// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "registers.h"

#include "core/core.h"
#include "core/arm/arm_interface.h"

RegistersWidget::RegistersWidget(QWidget* parent) : QDockWidget(parent) {
    cpu_regs_ui.setupUi(this);

    tree = cpu_regs_ui.treeWidget;
    tree->addTopLevelItem(registers = new QTreeWidgetItem(QStringList("Registers")));
    tree->addTopLevelItem(CSPR = new QTreeWidgetItem(QStringList("CSPR")));

    registers->setExpanded(true);
    CSPR->setExpanded(true);

    for (int i = 0; i < 16; ++i) {
        QTreeWidgetItem* child = new QTreeWidgetItem(QStringList(QString("R[%1]").arg(i, 2, 10, QLatin1Char('0'))));
        registers->addChild(child);
    }

    CSPR->addChild(new QTreeWidgetItem(QStringList("M")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("T")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("F")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("I")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("A")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("E")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("IT")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("GE")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("DNM")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("J")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("Q")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("V")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("C")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("Z")));
    CSPR->addChild(new QTreeWidgetItem(QStringList("N")));

    setEnabled(false);
}

void RegistersWidget::OnDebugModeEntered() {
    ARM_Interface* app_core = Core::g_app_core;

    if (app_core == nullptr)
        return;

    for (int i = 0; i < 16; ++i)
        registers->child(i)->setText(1, QString("0x%1").arg(app_core->GetReg(i), 8, 16, QLatin1Char('0')));

    CSPR->setText(1, QString("0x%1").arg(app_core->GetCPSR(), 8, 16, QLatin1Char('0')));
    CSPR->child(0)->setText(1, QString("b%1").arg(app_core->GetCPSR() & 0x1F, 5, 2, QLatin1Char('0'))); // M - Mode
    CSPR->child(1)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 5) & 0x1));    // T - State
    CSPR->child(2)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 6) & 0x1));    // F - FIQ disable
    CSPR->child(3)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 7) & 0x1));    // I - IRQ disable
    CSPR->child(4)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 8) & 0x1));    // A - Imprecise abort
    CSPR->child(5)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 9) & 0x1));    // E - Data endianess
    CSPR->child(6)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 10) & 0x3F));  // IT - If-Then state (DNM)
    CSPR->child(7)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 16) & 0xF));   // GE - Greater-than-or-Equal
    CSPR->child(8)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 20) & 0xF));   // DNM - Do not modify
    CSPR->child(9)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 24) & 0x1));   // J - Java state
    CSPR->child(10)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 27) & 0x1));  // Q - Sticky overflow
    CSPR->child(11)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 28) & 0x1));  // V - Overflow
    CSPR->child(12)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 29) & 0x1));  // C - Carry/Borrow/Extend
    CSPR->child(13)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 30) & 0x1));  // Z - Zero
    CSPR->child(14)->setText(1, QString("%1").arg((app_core->GetCPSR() >> 31) & 0x1));  // N - Negative/Less than
}

void RegistersWidget::OnDebugModeLeft() {
}

void RegistersWidget::OnEmulationStarting(EmuThread* emu_thread) {
    setEnabled(true);
}

void RegistersWidget::OnEmulationStopping() {
    // Reset widget text
    for (int i = 0; i < 16; ++i)
        registers->child(i)->setText(1, QString(""));

    for (int i = 0; i < 15; ++i)
        CSPR->child(i)->setText(1, QString(""));

    CSPR->setText(1, QString(""));

    setEnabled(false);
}
