// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "graphics_cmdlists.hxx"
#include <QTreeView>

extern GraphicsDebugger g_debugger;

GPUCommandListModel::GPUCommandListModel(QObject* parent) : QAbstractItemModel(parent)
{
    root_item = new TreeItem(TreeItem::ROOT, 0, NULL, this);

    connect(this, SIGNAL(CommandListCalled()), this, SLOT(OnCommandListCalledInternal()), Qt::UniqueConnection);
}

QModelIndex GPUCommandListModel::index(int row, int column, const QModelIndex& parent) const
{
    TreeItem* item;

    if (!parent.isValid()) {
        item = root_item;
    } else {
        item = (TreeItem*)parent.internalPointer();
    }

    return createIndex(row, column, item->children[row]);
}

QModelIndex GPUCommandListModel::parent(const QModelIndex& child) const
{
    if (!child.isValid())
        return QModelIndex();

    TreeItem* item = (TreeItem*)child.internalPointer();

    if (item->parent == NULL)
        return QModelIndex();

    return createIndex(item->parent->index, 0, item->parent);
}

int GPUCommandListModel::rowCount(const QModelIndex& parent) const
{
    TreeItem* item;
    if (!parent.isValid()) {
        item = root_item;
    } else {
        item = (TreeItem*)parent.internalPointer();
    }
    return item->children.size();
}

int GPUCommandListModel::columnCount(const QModelIndex& parent) const
{
    return 1;
}

QVariant GPUCommandListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const TreeItem* item = (const TreeItem*)index.internalPointer();

    if (item->type == TreeItem::COMMAND_LIST)
    {
        const GraphicsDebugger::PicaCommandList& cmdlist = command_lists[item->index].second;
        u32 address = command_lists[item->index].first;

        if (role == Qt::DisplayRole)
        {
            return QVariant(QString("0x%1 bytes at 0x%2").arg(cmdlist.size(), 0, 16).arg(address, 8, 16, QLatin1Char('0')));
        }
    }
    else
    {
        // index refers to a specific command
        const GraphicsDebugger::PicaCommandList& cmdlist = command_lists[item->parent->index].second;
        const GraphicsDebugger::PicaCommand& cmd = cmdlist[item->index];

        if (role == Qt::DisplayRole) {
            QString content;
            for (int j = 0; j < cmd.size(); ++j)
                content.append(QString("%1 ").arg(cmd[j], 8, 16, QLatin1Char('0')));

            return QVariant(content);
        }
    }

    return QVariant();
}

void GPUCommandListModel::OnCommandListCalled(const GraphicsDebugger::PicaCommandList& lst, bool is_new)
{
    emit CommandListCalled();
}


void GPUCommandListModel::OnCommandListCalledInternal()
{
    beginResetModel();

    command_lists = GetDebugger()->GetCommandLists();

    // delete root item and rebuild tree
    delete root_item;
    root_item = new TreeItem(TreeItem::ROOT, 0, NULL, this);

    for (int command_list_idx = 0; command_list_idx < command_lists.size(); ++command_list_idx) {
        TreeItem* command_list_item = new TreeItem(TreeItem::COMMAND_LIST, command_list_idx, root_item, root_item);
        root_item->children.push_back(command_list_item);

        const GraphicsDebugger::PicaCommandList& command_list = command_lists[command_list_idx].second;
        for (int command_idx = 0; command_idx < command_list.size(); ++command_idx) {
            TreeItem* command_item = new TreeItem(TreeItem::COMMAND, command_idx, command_list_item, command_list_item);
            command_list_item->children.push_back(command_item);
        }
    }

    endResetModel();
}

GPUCommandListWidget::GPUCommandListWidget(QWidget* parent) : QDockWidget(tr("Pica Command List"), parent)
{
    GPUCommandListModel* model = new GPUCommandListModel(this);
    g_debugger.RegisterObserver(model);

    QTreeView* tree_widget = new QTreeView;
    tree_widget->setModel(model);
    tree_widget->setFont(QFont("monospace"));
    setWidget(tree_widget);
}
