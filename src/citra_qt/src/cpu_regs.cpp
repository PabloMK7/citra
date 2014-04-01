#include "cpu_regs.hxx"

//#include "powerpc/cpu_core_regs.h"

GARM11RegsView::GARM11RegsView(QWidget* parent) : QDockWidget(parent)
{
    cpu_regs_ui.setupUi(this);

    tree = cpu_regs_ui.treeWidget;
    tree->addTopLevelItem(registers = new QTreeWidgetItem(QStringList("Registers")));
	tree->addTopLevelItem(CSPR = new QTreeWidgetItem(QStringList("CSPR")));

	//const Qt::ItemFlags child_flags = Qt::ItemIsEditable | Qt::ItemIsEnabled;
	//registers->setFlags(child_flags);
	//CSPR->setFlags(child_flags);

	for (int i = 0; i < 16; ++i)
    {
		QTreeWidgetItem* child = new QTreeWidgetItem(QStringList(QString("R[%1]").arg(i, 2, 10, QLatin1Char('0'))));
        //child->setFlags(child_flags);
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
}

void GARM11RegsView::OnCPUStepped()
{
	//TODO (Vail) replace values
	int value = 0;
	for (int i = 0; i < 16; ++i)
		registers->child(i)->setText(1, QString("0x%1").arg(i, 8, 16, QLatin1Char('0')));

	CSPR->child(0)->setText(1, QString("%1").arg(value));
	CSPR->child(1)->setText(1, QString("%1").arg(value));
	CSPR->child(2)->setText(1, QString("%1").arg(value));
	CSPR->child(3)->setText(1, QString("%1").arg(value));
	CSPR->child(4)->setText(1, QString("%1").arg(value));
	CSPR->child(5)->setText(1, QString("%1").arg(value));
	CSPR->child(6)->setText(1, QString("%1").arg(value));
	CSPR->child(7)->setText(1, QString("%1").arg(value));
	CSPR->child(8)->setText(1, QString("%1").arg(value));
	CSPR->child(9)->setText(1, QString("%1").arg(value));
	CSPR->child(10)->setText(1, QString("%1").arg(value));
	CSPR->child(11)->setText(1, QString("%1").arg(value));
	CSPR->child(12)->setText(1, QString("%1").arg(value));
	CSPR->child(13)->setText(1, QString("%1").arg(value));
	CSPR->child(14)->setText(1, QString("%1").arg(value));
	CSPR->child(15)->setText(1, QString("%1").arg(value));
}
