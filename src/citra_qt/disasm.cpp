#include <QtGui>
#include "ui_disasm.h"
#include "disasm.hxx"

#include "bootmanager.hxx"
#include "hotkeys.hxx"

#include "common/common.h"
#include "core/mem_map.h"

#include "core/core.h"
#include "common/break_points.h"
#include "common/symbols.h"
#include "core/arm/interpreter/armdefs.h"
#include "core/arm/disassembler/arm_disasm.h"

GDisAsmView::GDisAsmView(QWidget* parent, EmuThread& emu_thread) : QDockWidget(parent), base_addr(0), emu_thread(emu_thread)
{
    disasm_ui.setupUi(this);

	breakpoints = new BreakPoints();

    model = new QStandardItemModel(this);
    model->setColumnCount(3);
    disasm_ui.treeView->setModel(model);

    RegisterHotkey("Disassembler", "Start/Stop", QKeySequence(Qt::Key_F5), Qt::ApplicationShortcut);
    RegisterHotkey("Disassembler", "Step", QKeySequence(Qt::Key_F10), Qt::ApplicationShortcut);
    RegisterHotkey("Disassembler", "Step into", QKeySequence(Qt::Key_F11), Qt::ApplicationShortcut);
    RegisterHotkey("Disassembler", "Set Breakpoint", QKeySequence(Qt::Key_F9), Qt::ApplicationShortcut);

    connect(disasm_ui.button_breakpoint, SIGNAL(clicked()), this, SLOT(OnSetBreakpoint()));
    connect(disasm_ui.button_step, SIGNAL(clicked()), this, SLOT(OnStep()));
    connect(disasm_ui.button_pause, SIGNAL(clicked()), this, SLOT(OnPause()));
    connect(disasm_ui.button_continue, SIGNAL(clicked()), this, SLOT(OnContinue()));

    connect(GetHotkey("Disassembler", "Start/Stop", this), SIGNAL(activated()), this, SLOT(OnToggleStartStop()));
    connect(GetHotkey("Disassembler", "Step", this), SIGNAL(activated()), this, SLOT(OnStep()));
    connect(GetHotkey("Disassembler", "Step into", this), SIGNAL(activated()), this, SLOT(OnStepInto()));
    connect(GetHotkey("Disassembler", "Set Breakpoint", this), SIGNAL(activated()), this, SLOT(OnSetBreakpoint()));
}

void GDisAsmView::Init()
{
    ARM_Disasm* disasm = new ARM_Disasm();

    base_addr = Core::g_app_core->GetPC();
    unsigned int curInstAddr = base_addr;
    char result[255];

    for (int i = 0; i < 10000; i++) // fixed for now
    {
        disasm->disasm(curInstAddr, Memory::Read32(curInstAddr), result);
        model->setItem(i, 0, new QStandardItem(QString("0x%1").arg((uint)(curInstAddr), 8, 16, QLatin1Char('0'))));
        model->setItem(i, 1, new QStandardItem(QString(result)));
        if (Symbols::HasSymbol(curInstAddr))
        {
            TSymbol symbol = Symbols::GetSymbol(curInstAddr);
            model->setItem(i, 2, new QStandardItem(QString("%1 - Size:%2").arg(QString::fromStdString(symbol.name))
                                                                          .arg(symbol.size / 4))); // divide by 4 to get instruction count

        }
        curInstAddr += 4;
    }
    disasm_ui.treeView->resizeColumnToContents(0);
    disasm_ui.treeView->resizeColumnToContents(1);
    
    QModelIndex model_index = model->index(0, 0);
    disasm_ui.treeView->scrollTo(model_index);
    disasm_ui.treeView->selectionModel()->setCurrentIndex(model_index, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
}

void GDisAsmView::OnSetBreakpoint()
{
    int selected_row = SelectedRow();

    if (selected_row == -1)
        return;
	
    u32 address = base_addr + (selected_row * 4);
    if (breakpoints->IsAddressBreakPoint(address))
    {
		breakpoints->Remove(address);
        model->item(selected_row, 0)->setBackground(QBrush());
        model->item(selected_row, 1)->setBackground(QBrush());
    }
    else
    {
        breakpoints->Add(address);
        model->item(selected_row, 0)->setBackground(QBrush(QColor(0xFF, 0x99, 0x99)));
        model->item(selected_row, 1)->setBackground(QBrush(QColor(0xFF, 0x99, 0x99)));
    }
}

void GDisAsmView::OnContinue()
{
    emu_thread.SetCpuRunning(true);
}

void GDisAsmView::OnStep()
{
    OnStepInto(); // change later
}

void GDisAsmView::OnStepInto()
{
    emu_thread.SetCpuRunning(false);
    emu_thread.ExecStep();
}

void GDisAsmView::OnPause()
{
    emu_thread.SetCpuRunning(false);
}

void GDisAsmView::OnToggleStartStop()
{
    emu_thread.SetCpuRunning(!emu_thread.IsCpuRunning());
}

void GDisAsmView::OnCPUStepped()
{
    ARMword next_instr = Core::g_app_core->GetPC();

    if (breakpoints->IsAddressBreakPoint(next_instr))
    {
        emu_thread.SetCpuRunning(false);
    }

    unsigned int index = (next_instr - base_addr) / 4;
    QModelIndex model_index = model->index(index, 0);
    disasm_ui.treeView->scrollTo(model_index);
    disasm_ui.treeView->selectionModel()->setCurrentIndex(model_index, QItemSelectionModel::SelectCurrent | QItemSelectionModel::Rows);
}

int GDisAsmView::SelectedRow()
{
    QModelIndex index = disasm_ui.treeView->selectionModel()->currentIndex();
    if (!index.isValid())
        return -1;

    return model->itemFromIndex(disasm_ui.treeView->selectionModel()->currentIndex())->row();
}