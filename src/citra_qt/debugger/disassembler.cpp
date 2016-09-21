// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QShortcut>
#include "citra_qt/bootmanager.h"
#include "citra_qt/debugger/disassembler.h"
#include "citra_qt/hotkeys.h"
#include "citra_qt/util/util.h"
#include "common/break_points.h"
#include "common/symbols.h"
#include "core/arm/arm_interface.h"
#include "core/arm/disassembler/arm_disasm.h"
#include "core/core.h"
#include "core/memory.h"

DisassemblerModel::DisassemblerModel(QObject* parent)
    : QAbstractListModel(parent), base_address(0), code_size(0), program_counter(0),
      selection(QModelIndex()) {}

int DisassemblerModel::columnCount(const QModelIndex& parent) const {
    return 3;
}

int DisassemblerModel::rowCount(const QModelIndex& parent) const {
    return code_size;
}

QVariant DisassemblerModel::data(const QModelIndex& index, int role) const {
    switch (role) {
    case Qt::DisplayRole: {
        u32 address = base_address + index.row() * 4;
        u32 instr = Memory::Read32(address);
        std::string disassembly = ARM_Disasm::Disassemble(address, instr);

        if (index.column() == 0) {
            return QString("0x%1").arg((uint)(address), 8, 16, QLatin1Char('0'));
        } else if (index.column() == 1) {
            return QString::fromStdString(disassembly);
        } else if (index.column() == 2) {
            if (Symbols::HasSymbol(address)) {
                TSymbol symbol = Symbols::GetSymbol(address);
                return QString("%1 - Size:%2")
                    .arg(QString::fromStdString(symbol.name))
                    .arg(symbol.size / 4); // divide by 4 to get instruction count
            } else if (ARM_Disasm::Decode(instr) == OP_BL) {
                u32 offset = instr & 0xFFFFFF;

                // Sign-extend the 24-bit offset
                if ((offset >> 23) & 1)
                    offset |= 0xFF000000;

                // Pre-compute the left-shift and the prefetch offset
                offset <<= 2;
                offset += 8;

                TSymbol symbol = Symbols::GetSymbol(address + offset);
                return QString("    --> %1").arg(QString::fromStdString(symbol.name));
            }
        }

        break;
    }

    case Qt::BackgroundRole: {
        unsigned int address = base_address + 4 * index.row();

        if (breakpoints.IsAddressBreakPoint(address))
            return QBrush(QColor(0xFF, 0xC0, 0xC0));
        else if (address == program_counter)
            return QBrush(QColor(0xC0, 0xC0, 0xFF));

        break;
    }

    case Qt::FontRole: {
        if (index.column() == 0 || index.column() == 1) { // 2 is the symbols column
            return GetMonospaceFont();
        }
        break;
    }

    default:
        break;
    }

    return QVariant();
}

QModelIndex DisassemblerModel::IndexFromAbsoluteAddress(unsigned int address) const {
    return index((address - base_address) / 4, 0);
}

const BreakPoints& DisassemblerModel::GetBreakPoints() const {
    return breakpoints;
}

void DisassemblerModel::ParseFromAddress(unsigned int address) {

    // NOTE: A too large value causes lagging when scrolling the disassembly
    const unsigned int chunk_size = 1000 * 500;

    // If we haven't loaded anything yet, initialize base address to the parameter address
    if (code_size == 0)
        base_address = address;

    // If the new area is already loaded, just continue
    if (base_address + code_size > address + chunk_size && base_address <= address)
        return;

    // Insert rows before currently loaded data
    if (base_address > address) {
        unsigned int num_rows = (address - base_address) / 4;

        beginInsertRows(QModelIndex(), 0, num_rows);
        code_size += num_rows;
        base_address = address;

        endInsertRows();
    }

    // Insert rows after currently loaded data
    if (base_address + code_size < address + chunk_size) {
        unsigned int num_rows = (base_address + chunk_size - code_size - address) / 4;

        beginInsertRows(QModelIndex(), 0, num_rows);
        code_size += num_rows;
        endInsertRows();
    }

    SetNextInstruction(address);
}

void DisassemblerModel::OnSelectionChanged(const QModelIndex& new_selection) {
    selection = new_selection;
}

void DisassemblerModel::OnSetOrUnsetBreakpoint() {
    if (!selection.isValid())
        return;

    unsigned int address = base_address + selection.row() * 4;

    if (breakpoints.IsAddressBreakPoint(address)) {
        breakpoints.Remove(address);
    } else {
        breakpoints.Add(address);
    }

    emit dataChanged(selection, selection);
}

void DisassemblerModel::SetNextInstruction(unsigned int address) {
    QModelIndex cur_index = IndexFromAbsoluteAddress(program_counter);
    QModelIndex prev_index = IndexFromAbsoluteAddress(address);

    program_counter = address;

    emit dataChanged(cur_index, cur_index);
    emit dataChanged(prev_index, prev_index);
}

DisassemblerWidget::DisassemblerWidget(QWidget* parent, EmuThread* emu_thread)
    : QDockWidget(parent), base_addr(0), emu_thread(emu_thread) {

    disasm_ui.setupUi(this);

    RegisterHotkey("Disassembler", "Start/Stop", QKeySequence(Qt::Key_F5), Qt::ApplicationShortcut);
    RegisterHotkey("Disassembler", "Step", QKeySequence(Qt::Key_F10), Qt::ApplicationShortcut);
    RegisterHotkey("Disassembler", "Step into", QKeySequence(Qt::Key_F11), Qt::ApplicationShortcut);
    RegisterHotkey("Disassembler", "Set Breakpoint", QKeySequence(Qt::Key_F9),
                   Qt::ApplicationShortcut);

    connect(disasm_ui.button_step, SIGNAL(clicked()), this, SLOT(OnStep()));
    connect(disasm_ui.button_pause, SIGNAL(clicked()), this, SLOT(OnPause()));
    connect(disasm_ui.button_continue, SIGNAL(clicked()), this, SLOT(OnContinue()));

    connect(GetHotkey("Disassembler", "Start/Stop", this), SIGNAL(activated()), this,
            SLOT(OnToggleStartStop()));
    connect(GetHotkey("Disassembler", "Step", this), SIGNAL(activated()), this, SLOT(OnStep()));
    connect(GetHotkey("Disassembler", "Step into", this), SIGNAL(activated()), this,
            SLOT(OnStepInto()));

    setEnabled(false);
}

void DisassemblerWidget::Init() {
    model->ParseFromAddress(Core::g_app_core->GetPC());

    disasm_ui.treeView->resizeColumnToContents(0);
    disasm_ui.treeView->resizeColumnToContents(1);
    disasm_ui.treeView->resizeColumnToContents(2);

    QModelIndex model_index = model->IndexFromAbsoluteAddress(Core::g_app_core->GetPC());
    disasm_ui.treeView->scrollTo(model_index);
    disasm_ui.treeView->selectionModel()->setCurrentIndex(
        model_index, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
}

void DisassemblerWidget::OnContinue() {
    emu_thread->SetRunning(true);
}

void DisassemblerWidget::OnStep() {
    OnStepInto(); // change later
}

void DisassemblerWidget::OnStepInto() {
    emu_thread->SetRunning(false);
    emu_thread->ExecStep();
}

void DisassemblerWidget::OnPause() {
    emu_thread->SetRunning(false);

    // TODO: By now, the CPU might not have actually stopped...
    if (Core::g_app_core) {
        model->SetNextInstruction(Core::g_app_core->GetPC());
    }
}

void DisassemblerWidget::OnToggleStartStop() {
    emu_thread->SetRunning(!emu_thread->IsRunning());
}

void DisassemblerWidget::OnDebugModeEntered() {
    u32 next_instr = Core::g_app_core->GetPC();

    if (model->GetBreakPoints().IsAddressBreakPoint(next_instr))
        emu_thread->SetRunning(false);

    model->SetNextInstruction(next_instr);

    QModelIndex model_index = model->IndexFromAbsoluteAddress(next_instr);
    disasm_ui.treeView->scrollTo(model_index);
    disasm_ui.treeView->selectionModel()->setCurrentIndex(
        model_index, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
}

void DisassemblerWidget::OnDebugModeLeft() {}

int DisassemblerWidget::SelectedRow() {
    QModelIndex index = disasm_ui.treeView->selectionModel()->currentIndex();
    if (!index.isValid())
        return -1;

    return disasm_ui.treeView->selectionModel()->currentIndex().row();
}

void DisassemblerWidget::OnEmulationStarting(EmuThread* emu_thread) {
    this->emu_thread = emu_thread;

    model = new DisassemblerModel(this);
    disasm_ui.treeView->setModel(model);

    connect(disasm_ui.treeView->selectionModel(),
            SIGNAL(currentChanged(const QModelIndex&, const QModelIndex&)), model,
            SLOT(OnSelectionChanged(const QModelIndex&)));
    connect(disasm_ui.button_breakpoint, SIGNAL(clicked()), model, SLOT(OnSetOrUnsetBreakpoint()));
    connect(GetHotkey("Disassembler", "Set Breakpoint", this), SIGNAL(activated()), model,
            SLOT(OnSetOrUnsetBreakpoint()));

    Init();
    setEnabled(true);
}

void DisassemblerWidget::OnEmulationStopping() {
    disasm_ui.treeView->setModel(nullptr);
    delete model;
    emu_thread = nullptr;
    setEnabled(false);
}
