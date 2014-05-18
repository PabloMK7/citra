// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <QAbstractItemModel>
#include <QDockWidget>

#include "video_core/gpu_debugger.h"

// TODO: Rename class, since it's not actually a list model anymore...
class GPUCommandListModel : public QAbstractItemModel, public GraphicsDebugger::DebuggerObserver
{
    Q_OBJECT

public:
    GPUCommandListModel(QObject* parent);

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const;
    QModelIndex parent(const QModelIndex& child) const;
    int columnCount(const QModelIndex& parent = QModelIndex()) const;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

public:
    void OnCommandListCalled(const GraphicsDebugger::PicaCommandList& lst, bool is_new) override;

public slots:
    void OnCommandListCalledInternal();

signals:
    void CommandListCalled();

private:
    struct TreeItem : public QObject
    {
        enum Type {
            ROOT,
            COMMAND_LIST,
            COMMAND
        };

        TreeItem(Type type, int index, TreeItem* item_parent, QObject* parent) : QObject(parent), type(type), index(index), parent(item_parent) {}

        Type type;
        int index;
        std::vector<TreeItem*> children;
        TreeItem* parent;
    };

    std::vector<std::pair<u32,GraphicsDebugger::PicaCommandList>> command_lists;
    TreeItem* root_item;
};

class GPUCommandListWidget : public QDockWidget
{
    Q_OBJECT

public:
    GPUCommandListWidget(QWidget* parent = 0);

private:
};
