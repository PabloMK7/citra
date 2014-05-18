// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include "graphics_cmdlists.hxx"
#include <QListView>

extern GraphicsDebugger g_debugger;

GPUCommandListModel::GPUCommandListModel(QObject* parent) : QAbstractListModel(parent), row_count(0)
{
    connect(this, SIGNAL(CommandListCalled()), this, SLOT(OnCommandListCalledInternal()), Qt::UniqueConnection);
}

int GPUCommandListModel::rowCount(const QModelIndex& parent) const
{
    return row_count;
}

QVariant GPUCommandListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    int idx = index.row();
    if (role == Qt::DisplayRole)
    {
        QString content;
        const GraphicsDebugger::PicaCommandList& cmdlist = command_list[idx].second;
        for (int i = 0; i < cmdlist.size(); ++i)
        {
            const GraphicsDebugger::PicaCommand& cmd = cmdlist[i];
            for (int j = 0; j < cmd.size(); ++j)
                content.append(QString("%1 ").arg(cmd[j], 8, 16, QLatin1Char('0')));
        }
        return QVariant(content);
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

    command_list = GetDebugger()->GetCommandLists();
    row_count = command_list.size();

    endResetModel();
}

GPUCommandListWidget::GPUCommandListWidget(QWidget* parent) : QDockWidget(tr("Pica Command List"), parent)
{
    GPUCommandListModel* model = new GPUCommandListModel(this);
    g_debugger.RegisterObserver(model);

    QListView* list_widget = new QListView;
    list_widget->setModel(model);
    setWidget(list_widget);
}
