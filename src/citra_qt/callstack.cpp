#include <QStandardItemModel>
#include "callstack.hxx"

//#include "debugger/debugger.h"

GCallstackView::GCallstackView(QWidget* parent): QDockWidget(parent)
{
    ui.setupUi(this);

    callstack_model = new QStandardItemModel(this);
    callstack_model->setColumnCount(3);
    callstack_model->setHeaderData(0, Qt::Horizontal, "Depth");
    callstack_model->setHeaderData(1, Qt::Horizontal, "Address");
    callstack_model->setHeaderData(2, Qt::Horizontal, "Function Name");
    ui.treeView->setModel(callstack_model);

    // TODO: Make single clicking a callstack entry jump to the corresponding disassembly position
}

void GCallstackView::OnCPUStepped()
{
	/*
	Debugger::Callstack callstack;
    Debugger::GetCallstack(callstack);
    callstack_model->setRowCount(callstack.size());

    for (int i = 0; i < callstack.size(); ++i)
    for (Debugger::CallstackIterator it = callstack.begin(); it != callstack.end(); ++it)
    {
        Debugger::CallstackEntry entry = callstack[i];
        callstack_model->setItem(i, 0, new QStandardItem(QString("%1").arg(i+1)));
        callstack_model->setItem(i, 1, new QStandardItem(QString("0x%1").arg(entry.addr, 8, 16, QLatin1Char('0'))));
        callstack_model->setItem(i, 2, new QStandardItem(QString::fromStdString(entry.name)));
    }
	*/
}