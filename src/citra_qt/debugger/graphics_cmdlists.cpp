// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QApplication>
#include <QClipboard>
#include <QLabel>
#include <QListView>
#include <QMainWindow>
#include <QPushButton>
#include <QVBoxLayout>
#include <QTreeView>
#include <QHeaderView>
#include <QSpinBox>
#include <QComboBox>

#include "common/vector_math.h"

#include "video_core/debug_utils/debug_utils.h"
#include "video_core/pica.h"

#include "graphics_cmdlists.h"

#include "util/spinbox.h"

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

TextureInfoDockWidget::TextureInfoDockWidget(const Pica::DebugUtils::TextureInfo& info, QWidget* parent)
    : QDockWidget(tr("Texture 0x%1").arg(info.physical_address, 8, 16, QLatin1Char('0'))),
      info(info) {

    QWidget* main_widget = new QWidget;

    QLabel* image_widget = new QLabel;

    connect(this, SIGNAL(UpdatePixmap(const QPixmap&)), image_widget, SLOT(setPixmap(const QPixmap&)));

    CSpinBox* phys_address_spinbox = new CSpinBox;
    phys_address_spinbox->SetBase(16);
    phys_address_spinbox->SetRange(0, 0xFFFFFFFF);
    phys_address_spinbox->SetPrefix("0x");
    phys_address_spinbox->SetValue(info.physical_address);
    connect(phys_address_spinbox, SIGNAL(ValueChanged(qint64)), this, SLOT(OnAddressChanged(qint64)));

    QComboBox* format_choice = new QComboBox;
    format_choice->addItem(tr("RGBA8"));
    format_choice->addItem(tr("RGB8"));
    format_choice->addItem(tr("RGB5A1"));
    format_choice->addItem(tr("RGB565"));
    format_choice->addItem(tr("RGBA4"));
    format_choice->addItem(tr("IA8"));
    format_choice->addItem(tr("UNK6"));
    format_choice->addItem(tr("I8"));
    format_choice->addItem(tr("A8"));
    format_choice->addItem(tr("IA4"));
    format_choice->addItem(tr("I4"));
    format_choice->addItem(tr("A4"));
    format_choice->addItem(tr("ETC1"));
    format_choice->addItem(tr("ETC1A4"));
    format_choice->setCurrentIndex(static_cast<int>(info.format));
    connect(format_choice, SIGNAL(currentIndexChanged(int)), this, SLOT(OnFormatChanged(int)));

    QSpinBox* width_spinbox = new QSpinBox;
    width_spinbox->setMaximum(65535);
    width_spinbox->setValue(info.width);
    connect(width_spinbox, SIGNAL(valueChanged(int)), this, SLOT(OnWidthChanged(int)));

    QSpinBox* height_spinbox = new QSpinBox;
    height_spinbox->setMaximum(65535);
    height_spinbox->setValue(info.height);
    connect(height_spinbox, SIGNAL(valueChanged(int)), this, SLOT(OnHeightChanged(int)));

    QSpinBox* stride_spinbox = new QSpinBox;
    stride_spinbox->setMaximum(65535 * 4);
    stride_spinbox->setValue(info.stride);
    connect(stride_spinbox, SIGNAL(valueChanged(int)), this, SLOT(OnStrideChanged(int)));

    QVBoxLayout* main_layout = new QVBoxLayout;
    main_layout->addWidget(image_widget);

    {
        QHBoxLayout* sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Source Address:")));
        sub_layout->addWidget(phys_address_spinbox);
        main_layout->addLayout(sub_layout);
    }

    {
        QHBoxLayout* sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Format")));
        sub_layout->addWidget(format_choice);
        main_layout->addLayout(sub_layout);
    }

    {
        QHBoxLayout* sub_layout = new QHBoxLayout;
        sub_layout->addWidget(new QLabel(tr("Width:")));
        sub_layout->addWidget(width_spinbox);
        sub_layout->addStretch();
        sub_layout->addWidget(new QLabel(tr("Height:")));
        sub_layout->addWidget(height_spinbox);
        sub_layout->addStretch();
        sub_layout->addWidget(new QLabel(tr("Stride:")));
        sub_layout->addWidget(stride_spinbox);
        main_layout->addLayout(sub_layout);
    }

    main_widget->setLayout(main_layout);

    emit UpdatePixmap(ReloadPixmap());

    setWidget(main_widget);
}

void TextureInfoDockWidget::OnAddressChanged(qint64 value) {
    info.physical_address = value;
    emit UpdatePixmap(ReloadPixmap());
}

void TextureInfoDockWidget::OnFormatChanged(int value) {
    info.format = static_cast<Pica::Regs::TextureFormat>(value);
    emit UpdatePixmap(ReloadPixmap());
}

void TextureInfoDockWidget::OnWidthChanged(int value) {
    info.width = value;
    emit UpdatePixmap(ReloadPixmap());
}

void TextureInfoDockWidget::OnHeightChanged(int value) {
    info.height = value;
    emit UpdatePixmap(ReloadPixmap());
}

void TextureInfoDockWidget::OnStrideChanged(int value) {
    info.stride = value;
    emit UpdatePixmap(ReloadPixmap());
}

QPixmap TextureInfoDockWidget::ReloadPixmap() const {
    u8* src = Memory::GetPhysicalPointer(info.physical_address);
    return QPixmap::fromImage(LoadTexture(src, info));
}

GPUCommandListModel::GPUCommandListModel(QObject* parent) : QAbstractListModel(parent) {

}

int GPUCommandListModel::rowCount(const QModelIndex& parent) const {
    return static_cast<int>(pica_trace.writes.size());
}

int GPUCommandListModel::columnCount(const QModelIndex& parent) const {
    return 3;
}

QVariant GPUCommandListModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid())
        return QVariant();

    const auto& writes = pica_trace.writes;
    const Pica::CommandProcessor::CommandHeader cmd{writes[index.row()].Id()};
    const u32 val{writes[index.row()].Value()};

    if (role == Qt::DisplayRole) {
        QString content;
        switch ( index.column() ) {
        case 0:
            return QString::fromLatin1(Pica::Regs::GetCommandName(cmd.cmd_id).c_str());
        case 1:
            return QString("%1").arg(cmd.cmd_id, 3, 16, QLatin1Char('0'));
        case 2:
            return QString("%1").arg(val, 8, 16, QLatin1Char('0'));
        }
    } else if (role == CommandIdRole) {
        return QVariant::fromValue<int>(cmd.cmd_id.Value());
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
        } else {
            index = 2;
        }
        auto config = Pica::g_state.regs.GetTextures()[index].config;
        auto format = Pica::g_state.regs.GetTextures()[index].format;
        auto info = Pica::DebugUtils::TextureInfo::FromPicaRegister(config, format);

        // TODO: Instead, emit a signal here to be caught by the main window widget.
        auto main_window = static_cast<QMainWindow*>(parent());
        main_window->tabifyDockWidget(this, new TextureInfoDockWidget(info, main_window));
    }
}

void GPUCommandListWidget::SetCommandInfo(const QModelIndex& index) {
    QWidget* new_info_widget;

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
    } else {
        new_info_widget = new QWidget;
    }

    widget()->layout()->removeWidget(command_info_widget);
    delete command_info_widget;
    widget()->layout()->addWidget(new_info_widget);
    command_info_widget = new_info_widget;
}
#undef COMMAND_IN_RANGE

GPUCommandListWidget::GPUCommandListWidget(QWidget* parent) : QDockWidget(tr("Pica Command List"), parent) {
    setObjectName("Pica Command List");
    GPUCommandListModel* model = new GPUCommandListModel(this);

    QWidget* main_widget = new QWidget;

    list_widget = new QTreeView;
    list_widget->setModel(model);
    list_widget->setFont(QFont("monospace"));
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

    command_info_widget = new QWidget;

    QVBoxLayout* main_layout = new QVBoxLayout;
    main_layout->addWidget(list_widget);
    {
        QHBoxLayout* sub_layout = new QHBoxLayout;
        sub_layout->addWidget(toggle_tracing);
        sub_layout->addWidget(copy_all);
        main_layout->addLayout(sub_layout);
    }
    main_layout->addWidget(command_info_widget);
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

    QAbstractItemModel* model = static_cast<QAbstractListModel*>(list_widget->model());

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
