// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <QAbstractListModel>
#include <QDockWidget>

#include "video_core/gpu_debugger.h"

class GPUCommandListModel : public QAbstractListModel, public GraphicsDebugger::DebuggerObserver
{
    Q_OBJECT

public:
    GPUCommandListModel(QObject* parent);

    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

public:
    void OnCommandListCalled(const GraphicsDebugger::PicaCommandList& lst, bool is_new) override;

public slots:
    void OnCommandListCalledInternal();

signals:
    void CommandListCalled();

private:
    int row_count;
    std::vector<std::pair<u32,GraphicsDebugger::PicaCommandList>> command_list;
};

class GPUCommandListWidget : public QDockWidget
{
    Q_OBJECT

public:
    GPUCommandListWidget(QWidget* parent = 0);

private:
};
