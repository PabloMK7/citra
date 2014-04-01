#include <QDockWidget>
#include "ui_disasm.h"

#include "common.h"
#include "break_points.h"

class QAction;
class QStandardItemModel;
class EmuThread;

class GDisAsmView : public QDockWidget
{
    Q_OBJECT

public:
    GDisAsmView(QWidget* parent, EmuThread& emu_thread);

public slots:
    void OnSetBreakpoint();
    void OnStep();
    void OnPause();
    void OnContinue();

    void OnCPUStepped();

private:
    // returns -1 if no row is selected
    int SelectedRow();

    Ui::DockWidget disasm_ui;
    QStandardItemModel* model;

    u32 base_addr;

	BreakPoints* breakpoints;

    EmuThread& emu_thread;
};
