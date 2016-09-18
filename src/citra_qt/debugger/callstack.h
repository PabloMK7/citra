// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QDockWidget>
#include "ui_callstack.h"

class QStandardItemModel;

class CallstackWidget : public QDockWidget {
    Q_OBJECT

public:
    CallstackWidget(QWidget* parent = nullptr);

public slots:
    void OnDebugModeEntered();
    void OnDebugModeLeft();

private:
    Ui::CallStack ui;
    QStandardItemModel* callstack_model;

    /// Clears the callstack widget while keeping the column widths the same
    void Clear();
};
