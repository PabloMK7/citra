// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTreeView>

#include "graphics_cmdlists.hxx"

#include "video_core/pica.h"
#include "video_core/math.h"

#include "video_core/debug_utils/debug_utils.h"

class TextureInfoWidget : public QWidget {
public:
    TextureInfoWidget(u8* src, const Pica::DebugUtils::TextureInfo& info, QWidget* parent = nullptr) : QWidget(parent) {
        QImage decoded_image(info.width, info.height, QImage::Format_ARGB32);
        for (int y = 0; y < info.height; ++y) {
            for (int x = 0; x < info.width; ++x) {
                Math::Vec4<u8> color = Pica::DebugUtils::LookupTexture(src, x, y, info);
                decoded_image.setPixel(x, y, qRgba(color.r(), color.g(), color.b(), color.a()));
            }
        }

        QLabel* image_widget = new QLabel;
        QPixmap image_pixmap = QPixmap::fromImage(decoded_image);
        image_pixmap = image_pixmap.scaled(200, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        image_widget->setPixmap(image_pixmap);

        QVBoxLayout* layout = new QVBoxLayout;
        layout->addWidget(image_widget);
        setLayout(layout);
    }
};

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
    } else if (role == CommandIdRole) {
        return QVariant::fromValue<int>(cmd.cmd_id.Value());
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

void GPUCommandListWidget::SetCommandInfo(const QModelIndex& index)
{
    QWidget* new_info_widget;

#define COMMAND_IN_RANGE(cmd_id, reg_name) (cmd_id >= PICA_REG_INDEX(reg_name) && cmd_id < PICA_REG_INDEX(reg_name) + sizeof(decltype(Pica::registers.reg_name)) / 4)
    const int command_id = list_widget->model()->data(index, GPUCommandListModel::CommandIdRole).toInt();
    if (COMMAND_IN_RANGE(command_id, texture0)) {
        u8* src = Memory::GetPointer(Pica::registers.texture0.GetPhysicalAddress());
        Pica::DebugUtils::TextureInfo info;
        info.width = Pica::registers.texture0.width;
        info.height = Pica::registers.texture0.height;
        info.stride = 3 * Pica::registers.texture0.width;
        info.format = Pica::registers.texture0_format;
        new_info_widget = new TextureInfoWidget(src, info);
    } else {
        new_info_widget = new QWidget;
    }
#undef COMMAND_IN_RANGE

    widget()->layout()->removeWidget(command_info_widget);
    delete command_info_widget;
    widget()->layout()->addWidget(new_info_widget);
    command_info_widget = new_info_widget;
}

GPUCommandListWidget::GPUCommandListWidget(QWidget* parent) : QDockWidget(tr("Pica Command List"), parent)
{
    setObjectName("Pica Command List");
    GPUCommandListModel* model = new GPUCommandListModel(this);

    QWidget* main_widget = new QWidget;

    list_widget = new QTreeView;
    list_widget->setModel(model);
    list_widget->setFont(QFont("monospace"));
    list_widget->setRootIsDecorated(false);

    connect(list_widget->selectionModel(), SIGNAL(currentChanged(const QModelIndex&,const QModelIndex&)),
            this, SLOT(SetCommandInfo(const QModelIndex&)));


    toggle_tracing = new QPushButton(tr("Start Tracing"));

    connect(toggle_tracing, SIGNAL(clicked()), this, SLOT(OnToggleTracing()));
    connect(this, SIGNAL(TracingFinished(const Pica::DebugUtils::PicaTrace&)),
            model, SLOT(OnPicaTraceFinished(const Pica::DebugUtils::PicaTrace&)));

    command_info_widget = new QWidget;

    QVBoxLayout* main_layout = new QVBoxLayout;
    main_layout->addWidget(list_widget);
    main_layout->addWidget(toggle_tracing);
    main_layout->addWidget(command_info_widget);
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
