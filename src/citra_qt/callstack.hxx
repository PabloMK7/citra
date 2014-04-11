#include <QDockWidget>
#include "ui_callstack.h"
#include "common/platform.h"

class QStandardItemModel;

class GCallstackView : public QDockWidget
{
    Q_OBJECT

public:
    GCallstackView(QWidget* parent = 0);

public slots:
    void OnCPUStepped();

private:
    Ui::CallStack ui;
    QStandardItemModel* callstack_model;
};
