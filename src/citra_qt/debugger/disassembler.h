// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QAbstractListModel>
#include <QDockWidget>

#include "ui_disassembler.h"

#include "common/break_points.h"
#include "common/common_types.h"

class QAction;
class EmuThread;

class DisassemblerModel : public QAbstractListModel
{
    Q_OBJECT

public:
    DisassemblerModel(QObject* parent);

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

    QModelIndex IndexFromAbsoluteAddress(unsigned int address) const;
    const BreakPoints& GetBreakPoints() const;

public slots:
    void ParseFromAddress(unsigned int address);
    void OnSelectionChanged(const QModelIndex&);
    void OnSetOrUnsetBreakpoint();
    void SetNextInstruction(unsigned int address);

private:
    unsigned int base_address;
    unsigned int code_size;
    unsigned int program_counter;

    QModelIndex selection;

    // TODO: Make BreakPoints less crappy (i.e. const-correct) so that this needn't be mutable.
    mutable BreakPoints breakpoints;
};

class DisassemblerWidget : public QDockWidget
{
    Q_OBJECT

public:
    DisassemblerWidget(QWidget* parent, EmuThread* emu_thread);

    void Init();

public slots:
    void OnContinue();
    void OnStep();
    void OnStepInto();
    void OnPause();
    void OnToggleStartStop();

    void OnDebugModeEntered();
    void OnDebugModeLeft();

    void OnEmulationStarting(EmuThread* emu_thread);
    void OnEmulationStopping();

private:
    // returns -1 if no row is selected
    int SelectedRow();

    Ui::DockWidget disasm_ui;

    DisassemblerModel* model;

    u32 base_addr;

    EmuThread* emu_thread;
};
