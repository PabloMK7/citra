#include <QtGui>
#include "ui_disasm.h"
#include "disasm.hxx"

#include "bootmanager.hxx"
#include "hotkeys.hxx"

#include "common.h"
#include "mem_map.h"

#include "break_points.h"
//#include "powerpc/cpu_core_regs.h"
#include "arm/disassembler/arm_disasm.h"

//#include "powerpc/interpreter/cpu_int.h"

GDisAsmView::GDisAsmView(QWidget* parent, EmuThread& emu_thread) : QDockWidget(parent), base_addr(0), emu_thread(emu_thread)
{
    disasm_ui.setupUi(this);

	breakpoints = new BreakPoints();

    model = new QStandardItemModel(this);
    model->setColumnCount(3);
    disasm_ui.treeView->setModel(model);

    RegisterHotkey("Disassembler", "Step", QKeySequence(Qt::Key_F10), Qt::ApplicationShortcut);
    RegisterHotkey("Disassembler", "Step into", QKeySequence(Qt::Key_F11), Qt::ApplicationShortcut);
//    RegisterHotkey("Disassembler", "Pause", QKeySequence(Qt::Key_F5), Qt::ApplicationShortcut);
    RegisterHotkey("Disassembler", "Continue", QKeySequence(Qt::Key_F5), Qt::ApplicationShortcut);
    RegisterHotkey("Disassembler", "Set Breakpoint", QKeySequence(Qt::Key_F9), Qt::ApplicationShortcut);

    connect(disasm_ui.button_breakpoint, SIGNAL(clicked()), this, SLOT(OnSetBreakpoint()));
    connect(disasm_ui.button_step, SIGNAL(clicked()), this, SLOT(OnStep()));
    connect(disasm_ui.button_pause, SIGNAL(clicked()), this, SLOT(OnPause()));
    connect(disasm_ui.button_continue, SIGNAL(clicked()), this, SLOT(OnContinue()));
    connect(GetHotkey("Disassembler", "Step", this), SIGNAL(activated()), this, SLOT(OnStep()));
    connect(GetHotkey("Disassembler", "Step into", this), SIGNAL(activated()), this, SLOT(OnStepInto()));
//    connect(GetHotkey("Disassembler", "Pause", this), SIGNAL(activated()), this, SLOT(OnPause()));
    connect(GetHotkey("Disassembler", "Continue", this), SIGNAL(activated()), this, SLOT(OnContinue()));
    connect(GetHotkey("Disassembler", "Set Breakpoint", this), SIGNAL(activated()), this, SLOT(OnSetBreakpoint()));
}

void GDisAsmView::OnSetBreakpoint()
{
	if (SelectedRow() == -1)
        return;
	
    u32 address = base_addr + 4 * SelectedRow();
    if (breakpoints->IsAddressBreakPoint(address))
    {
		breakpoints->Remove(address);
        model->item(SelectedRow(), 0)->setBackground(QBrush());
        model->item(SelectedRow(), 1)->setBackground(QBrush());
        model->item(SelectedRow(), 2)->setBackground(QBrush());
    }
    else
    {
        breakpoints->Add(address);
        model->item(SelectedRow(), 0)->setBackground(Qt::red);
        model->item(SelectedRow(), 1)->setBackground(Qt::red);
        model->item(SelectedRow(), 2)->setBackground(Qt::red);
    }
}

void GDisAsmView::OnStep()
{
    emu_thread.SetCpuRunning(false);
    emu_thread.ExecStep();
}

void GDisAsmView::OnPause()
{
    emu_thread.SetCpuRunning(false);
}

void GDisAsmView::OnContinue()
{
    emu_thread.SetCpuRunning(true);
}

void GDisAsmView::OnCPUStepped()
{
	/*
	base_addr = ireg.PC - 52;
    unsigned int curInstAddr = base_addr;
    int  counter = 0;
    QModelIndex cur_instr_index;
    model->setRowCount(100);
    while(true)
    {
        u32 opcode = *(u32*)(&Mem_RAM[curInstAddr & RAM_MASK]);

        char out1[64];
        char out2[128];
        u32 out3 = 0;
        memset(out1, 0, sizeof(out1));
        memset(out2, 0, sizeof(out2));

        // NOTE: out3 (nextInstAddr) seems to be bugged, better don't use it...
        DisassembleGekko(out1, out2, opcode, curInstAddr, &out3);
        model->setItem(counter, 0, new QStandardItem(QString("0x%1").arg((uint)curInstAddr, 8, 16, QLatin1Char('0'))));
        model->setItem(counter, 1, new QStandardItem(QString(out1)));
        model->setItem(counter, 2, new QStandardItem(QString(out2)));

        if (ireg.PC == curInstAddr)
        {
            model->item(counter, 0)->setBackground(Qt::yellow);
            model->item(counter, 1)->setBackground(Qt::yellow);
            model->item(counter, 2)->setBackground(Qt::yellow);
            cur_instr_index = model->index(counter, 0);
        }
        else if (Debugger::IsBreakpoint(curInstAddr))
        {
            model->item(counter, 0)->setBackground(Qt::red);
            model->item(counter, 1)->setBackground(Qt::red);
            model->item(counter, 2)->setBackground(Qt::red);
        }
        else
        {
            model->item(counter, 0)->setBackground(QBrush());
            model->item(counter, 1)->setBackground(QBrush());
            model->item(counter, 2)->setBackground(QBrush());
        }
        curInstAddr += 4;

        ++counter;
        if (counter >= 100) break;
    }
    disasm_ui.treeView->resizeColumnToContents(0);
    disasm_ui.treeView->resizeColumnToContents(1);
    disasm_ui.treeView->resizeColumnToContents(2);
    disasm_ui.treeView->scrollTo(cur_instr_index); // QAbstractItemView::PositionAtCenter?
	*/
}

int GDisAsmView::SelectedRow()
{
    QModelIndex index = disasm_ui.treeView->selectionModel()->currentIndex();
    if (!index.isValid())
        return -1;

    return model->itemFromIndex(disasm_ui.treeView->selectionModel()->currentIndex())->row();
}