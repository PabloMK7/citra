// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QListView>
#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QTreeView>
#include <QVBoxLayout>

#include "citra_qt/debugger/graphics_cmdlists.h"
#include "citra_qt/util/spinbox.h"
#include "citra_qt/util/util.h"

#include "common/vector_math.h"

#include "video_core/pica.h"
#include "video_core/pica_state.h"
#include "video_core/debug_utils/debug_utils.h"

QImage LoadTexture(u8* src, const Pica::DebugUtils::TextureInfo& info) {
    QImage decoded_image(info.width, info.height, QImage::Format_ARGB32);
    for (int y = 0; y < info.height; ++y) {
        for (int x = 0; x < info.width; ++x) {
            Math::Vec4<u8> color = Pica::DebugUtils::LookupTexture(src, x, y, info, true);
            decoded_image.setPixel(x, y, qRgba(color.r(), color.g(), color.b(), color.a()));
        }
    }

    return decoded_image;
}

class TextureInfoWidget : public QWidget {
public:
    TextureInfoWidget(u8* src, const Pica::DebugUtils::TextureInfo& info, QWidget* parent = nullptr) : QWidget(parent) {
        QLabel* image_widget = new QLabel;
        QPixmap image_pixmap = QPixmap::fromImage(LoadTexture(src, info));
        image_pixmap = image_pixmap.scaled(200, 100, Qt::KeepAspectRatio, Qt::SmoothTransformation);
        image_widget->setPixmap(image_pixmap);

        QVBoxLayout* layout = new QVBoxLayout;
        layout->addWidget(image_widget);
        setLayout(layout);
    }
};

GPUCommandListModel::GPUCommandListModel(QObject* parent) : QAbstractListModel(parent) {

}

int GPUCommandListModel::rowCount(const QModelIndex& parent) const {
    return static_cast<int>(pica_trace.writes.size());
}

int GPUCommandListModel::columnCount(const QModelIndex& parent) const {
    return 4;
}

QVariant GPUCommandListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return QVariant();

    const auto& write = pica_trace.writes[index.row()];

    if (role == Qt::DisplayRole) {
        QString content;
        switch ( index.column() ) {
        case 0:
            return QString::fromLatin1(Pica::Regs::GetCommandName(write.cmd_id).c_str());
        case 1:
            return QString("%1").arg(write.cmd_id, 3, 16, QLatin1Char('0'));
        case 2:
            return QString("%1").arg(write.mask, 4, 2, QLatin1Char('0'));
        case 3:
            return QString("%1").arg(write.value, 8, 16, QLatin1Char('0'));
        }
    } else if (role == CommandIdRole) {
        return QVariant::fromValue<int>(write.cmd_id);
    }

    return QVariant();
}

QVariant GPUCommandListModel::headerData(int section, Qt::Orientation orientation, int role) const {
    switch(role) {
    case Qt::DisplayRole:
    {
        switch (section) {
        case 0:
            return tr("Command Name");
        case 1:
            return tr("Register");
        case 2:
            return tr("Mask");
        case 3:
            return tr("New Value");
        }

        break;
    }
    }

    return QVariant();
}

void GPUCommandListModel::OnPicaTraceFinished(const Pica::DebugUtils::PicaTrace& trace) {
    beginResetModel();

    pica_trace = trace;

    endResetModel();
}

#define COMMAND_IN_RANGE(cmd_id, reg_name)   \
    (cmd_id >= PICA_REG_INDEX(reg_name) &&   \
     cmd_id < PICA_REG_INDEX(reg_name) + sizeof(decltype(Pica::g_state.regs.reg_name)) / 4)

void GPUCommandListWidget::OnCommandDoubleClicked(const QModelIndex& index) {
    const unsigned int command_id = list_widget->model()->data(index, GPUCommandListModel::CommandIdRole).toUInt();
    if (COMMAND_IN_RANGE(command_id, texture0) ||
        COMMAND_IN_RANGE(command_id, texture1) ||
        COMMAND_IN_RANGE(command_id, texture2)) {

        unsigned index;
        if (COMMAND_IN_RANGE(command_id, texture0)) {
            index = 0;
        } else if (COMMAND_IN_RANGE(command_id, texture1)) {
            index = 1;
        } else if (COMMAND_IN_RANGE(command_id, texture2)) {
            index = 2;
        } else {
            UNREACHABLE_MSG("Unknown texture command");
        }
        auto config = Pica::g_state.regs.GetTextures()[index].config;
        auto format = Pica::g_state.regs.GetTextures()[index].format;
        auto info = Pica::DebugUtils::TextureInfo::FromPicaRegister(config, format);

        // TODO: Open a surface debugger
    }
}

void GPUCommandListWidget::SetCommandInfo(const QModelIndex& index) {
    QWidget* new_info_widget = nullptr;

    const unsigned int command_id = list_widget->model()->data(index, GPUCommandListModel::CommandIdRole).toUInt();
    if (COMMAND_IN_RANGE(command_id, texture0) ||
        COMMAND_IN_RANGE(command_id, texture1) ||
        COMMAND_IN_RANGE(command_id, texture2)) {

        unsigned index;
        if (COMMAND_IN_RANGE(command_id, texture0)) {
            index = 0;
        } else if (COMMAND_IN_RANGE(command_id, texture1)) {
            index = 1;
        } else {
            index = 2;
        }
        auto config = Pica::g_state.regs.GetTextures()[index].config;
        auto format = Pica::g_state.regs.GetTextures()[index].format;

        auto info = Pica::DebugUtils::TextureInfo::FromPicaRegister(config, format);
        u8* src = Memory::GetPhysicalPointer(config.GetPhysicalAddress());
        new_info_widget = new TextureInfoWidget(src, info);
    }
    if (command_info_widget) {
        delete command_info_widget;
        command_info_widget = nullptr;
    }
    if (new_info_widget) {
        widget()->layout()->addWidget(new_info_widget);
        command_info_widget = new_info_widget;
    }
}
#undef COMMAND_IN_RANGE

GPUCommandListWidget::GPUCommandListWidget(QWidget* parent) : QDockWidget(tr("Pica Command List"), parent) {
    setObjectName("Pica Command List");
    GPUCommandListModel* model = new GPUCommandListModel(this);

    QWidget* main_widget = new QWidget;

    list_widget = new QTreeView;
    list_widget->setModel(model);
    list_widget->setFont(GetMonospaceFont());
    list_widget->setRootIsDecorated(false);
    list_widget->setUniformRowHeights(true);

#if QT_VERSION >= QT_VERSION_CHECK(5,0,0)
    list_widget->header()->setSectionResizeMode(QHeaderView::ResizeToContents);
#else
    list_widget->header()->setResizeMode(QHeaderView::ResizeToContents);
#endif

    connect(list_widget->selectionModel(), SIGNAL(currentChanged(const QModelIndex&,const QModelIndex&)),
            this, SLOT(SetCommandInfo(const QModelIndex&)));
    connect(list_widget, SIGNAL(doubleClicked(const QModelIndex&)),
            this, SLOT(OnCommandDoubleClicked(const QModelIndex&)));

    toggle_tracing = new QPushButton(tr("Start Tracing"));
    QPushButton* copy_all = new QPushButton(tr("Copy All"));

    connect(toggle_tracing, SIGNAL(clicked()), this, SLOT(OnToggleTracing()));
    connect(this, SIGNAL(TracingFinished(const Pica::DebugUtils::PicaTrace&)),
            model, SLOT(OnPicaTraceFinished(const Pica::DebugUtils::PicaTrace&)));

    connect(copy_all, SIGNAL(clicked()), this, SLOT(CopyAllToClipboard()));

    command_info_widget = nullptr;

    QVBoxLayout* main_layout = new QVBoxLayout;
    main_layout->addWidget(list_widget);
    {
        QHBoxLayout* sub_layout = new QHBoxLayout;
        sub_layout->addWidget(toggle_tracing);
        sub_layout->addWidget(copy_all);
        main_layout->addLayout(sub_layout);
    }
    main_widget->setLayout(main_layout);

    setWidget(main_widget);
}

void GPUCommandListWidget::OnToggleTracing() {
    if (!Pica::DebugUtils::IsPicaTracing()) {
        Pica::DebugUtils::StartPicaTracing();
        toggle_tracing->setText(tr("Finish Tracing"));
    } else {
        pica_trace = Pica::DebugUtils::FinishPicaTracing();
        emit TracingFinished(*pica_trace);
        toggle_tracing->setText(tr("Start Tracing"));
    }
}

void GPUCommandListWidget::CopyAllToClipboard() {
    QClipboard* clipboard = QApplication::clipboard();
    QString text;

    QAbstractItemModel* model = static_cast<QAbstractItemModel*>(list_widget->model());

    for (int row = 0; row < model->rowCount({}); ++row) {
        for (int col = 0; col < model->columnCount({}); ++col) {
            QModelIndex index = model->index(row, col);
            text += model->data(index).value<QString>();
            text += '\t';
        }
        text += '\n';
    }

    clipboard->setText(text);
}
