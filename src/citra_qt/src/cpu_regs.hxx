#include "ui_cpu_regs.h"

#include <QDockWidget>
#include <QTreeWidgetItem>

//#include "ui_gekko_regs.h"

class QTreeWidget;

class GARM11RegsView : public QDockWidget
{
    Q_OBJECT

public:
    GARM11RegsView(QWidget* parent = NULL);

public slots:
    void OnCPUStepped();

private:
	Ui::ARMRegisters cpu_regs_ui;

	QTreeWidget* tree;
    
	QTreeWidgetItem* registers;
	QTreeWidgetItem* CSPR;
};
