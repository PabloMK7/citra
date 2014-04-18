#include <QDockWidget>
#include "../ui_callstack.h"

class QStandardItemModel;

class CallstackWidget : public QDockWidget
{
    Q_OBJECT

public:
    CallstackWidget(QWidget* parent = 0);

public slots:
    void OnCPUStepped();

private:
    Ui::CallStack ui;
    QStandardItemModel* callstack_model;
};
