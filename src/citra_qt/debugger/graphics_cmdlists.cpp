// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <QListView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTreeView>

#include "graphics_cmdlists.hxx"

GPUCommandListModel::GPUCommandListModel(QObject* parent) : QAbstractListModel(parent)
{

}

int GPUCommandListModel::rowCount(const QModelIndex& parent) const
{
    return pica_trace.writes.size();
}

int GPUCommandListModel::columnCount(const QModelIndex& parent) const
{
    return 2;
}

QVariant GPUCommandListModel::data(const QModelIndex& index, int role) const
{
    if (!index.isValid())
        return QVariant();

    const auto& writes = pica_trace.writes;
    const Pica::CommandProcessor::CommandHeader cmd{writes[index.row()].Id()};
    const u32 val{writes[index.row()].Value()};

    if (role == Qt::DisplayRole) {
        QString content;
        if (index.column() == 0) {
            content = QString::fromLatin1(Pica::Regs::GetCommandName(cmd.cmd_id).c_str());
            content.append(" ");
        } else if (index.column() == 1) {
            content.append(QString("%1 ").arg(cmd.hex, 8, 16, QLatin1Char('0')));
            content.append(QString("%1 ").arg(val, 8, 16, QLatin1Char('0')));
        }

        return QVariant(content);
    }

    return QVariant();
}

QVariant GPUCommandListModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    switch(role) {
    case Qt::DisplayRole:
    {
        if (section == 0) {
            return tr("Command Name");
        } else if (section == 1) {
            return tr("Data");
        }

        break;
    }
    }

    return QVariant();
}

void GPUCommandListModel::OnPicaTraceFinished(const Pica::DebugUtils::PicaTrace& trace)
{
    beginResetModel();

    pica_trace = trace;

    endResetModel();
}


GPUCommandListWidget::GPUCommandListWidget(QWidget* parent) : QDockWidget(tr("Pica Command List"), parent)
{
    GPUCommandListModel* model = new GPUCommandListModel(this);

    QWidget* main_widget = new QWidget;

    QTreeView* list_widget = new QTreeView;
    list_widget->setModel(model);
    list_widget->setFont(QFont("monospace"));
    list_widget->setRootIsDecorated(false);

    toggle_tracing = new QPushButton(tr("Start Tracing"));

    connect(toggle_tracing, SIGNAL(clicked()), this, SLOT(OnToggleTracing()));
    connect(this, SIGNAL(TracingFinished(const Pica::DebugUtils::PicaTrace&)),
            model, SLOT(OnPicaTraceFinished(const Pica::DebugUtils::PicaTrace&)));

    QVBoxLayout* main_layout = new QVBoxLayout;
    main_layout->addWidget(list_widget);
    main_layout->addWidget(toggle_tracing);
    main_widget->setLayout(main_layout);

    setWidget(main_widget);
}

void GPUCommandListWidget::OnToggleTracing()
{
    if (!Pica::DebugUtils::IsPicaTracing()) {
        Pica::DebugUtils::StartPicaTracing();
        toggle_tracing->setText(tr("Stop Tracing"));
    } else {
        pica_trace = Pica::DebugUtils::FinishPicaTracing();
        emit TracingFinished(*pica_trace);
        toggle_tracing->setText(tr("Start Tracing"));
    }
}
