#include <QStandardItemModel>

#include "callstack.hxx"

#include "core/core.h"
#include "core/arm/arm_interface.h"
#include "core/mem_map.h"
#include "common/symbols.h"
#include "core/arm/disassembler/arm_disasm.h"

CallstackWidget::CallstackWidget(QWidget* parent): QDockWidget(parent)
{
    ui.setupUi(this);

    callstack_model = new QStandardItemModel(this);
    callstack_model->setColumnCount(4);
    callstack_model->setHeaderData(0, Qt::Horizontal, "Stack pointer");
    callstack_model->setHeaderData(2, Qt::Horizontal, "Return address");
    callstack_model->setHeaderData(1, Qt::Horizontal, "Call address");
    callstack_model->setHeaderData(3, Qt::Horizontal, "Function");
    ui.treeView->setModel(callstack_model);
}

void CallstackWidget::OnCPUStepped()
{
    ARM_Disasm* disasm = new ARM_Disasm();
    ARM_Interface* app_core = Core::g_app_core;

    u32 sp = app_core->GetReg(13); //stack pointer
    u32 addr, ret_addr, call_addr, func_addr;
    
    int counter = 0;
    for (int addr = 0x10000000; addr >= sp; addr -= 4)
    {
        ret_addr = Memory::Read32(addr);
        call_addr = ret_addr - 4; //get call address???

        /* TODO (mattvail) clean me, move to debugger interface */
        u32 insn = Memory::Read32(call_addr);
        if (disasm->decode(insn) == OP_BL)
        {
            std::string name;
            // ripped from disasm
            uint8_t cond = (insn >> 28) & 0xf;
            uint32_t i_offset = insn & 0xffffff;
            // Sign-extend the 24-bit offset
            if ((i_offset >> 23) & 1)
                i_offset |= 0xff000000;

            // Pre-compute the left-shift and the prefetch offset
            i_offset <<= 2;
            i_offset += 8;
            func_addr = call_addr + i_offset;

            callstack_model->setItem(counter, 0, new QStandardItem(QString("0x%1").arg(addr, 8, 16, QLatin1Char('0'))));
            callstack_model->setItem(counter, 1, new QStandardItem(QString("0x%1").arg(ret_addr, 8, 16, QLatin1Char('0'))));
            callstack_model->setItem(counter, 2, new QStandardItem(QString("0x%1").arg(call_addr, 8, 16, QLatin1Char('0'))));
            
            name = Symbols::HasSymbol(func_addr) ? Symbols::GetSymbol(func_addr).name : "unknown";
            callstack_model->setItem(counter, 3, new QStandardItem(QString("%1_%2").arg(QString::fromStdString(name))
                .arg(QString("0x%1").arg(func_addr, 8, 16, QLatin1Char('0')))));

            counter++;
        }
    }
}