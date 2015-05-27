// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QAbstractListModel>
#include <QDockWidget>

#include "video_core/gpu_debugger.h"
#include "video_core/debug_utils/debug_utils.h"

class QPushButton;
class QTreeView;

class GPUCommandListModel : public QAbstractListModel
{
    Q_OBJECT

public:
    enum {
        CommandIdRole = Qt::UserRole,
    };

    GPUCommandListModel(QObject* parent);

    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public slots:
    void OnPicaTraceFinished(const Pica::DebugUtils::PicaTrace& trace);

private:
    Pica::DebugUtils::PicaTrace pica_trace;
};

class GPUCommandListWidget : public QDockWidget
{
    Q_OBJECT

public:
    GPUCommandListWidget(QWidget* parent = 0);

public slots:
    void OnToggleTracing();
    void OnCommandDoubleClicked(const QModelIndex&);

    void SetCommandInfo(const QModelIndex&);

    void CopyAllToClipboard();

signals:
    void TracingFinished(const Pica::DebugUtils::PicaTrace&);

private:
    std::unique_ptr<Pica::DebugUtils::PicaTrace> pica_trace;

    QTreeView* list_widget;
    QWidget* command_info_widget;
    QPushButton* toggle_tracing;
};

class TextureInfoDockWidget : public QDockWidget {
    Q_OBJECT

public:
    TextureInfoDockWidget(const Pica::DebugUtils::TextureInfo& info, QWidget* parent = nullptr);

signals:
    void UpdatePixmap(const QPixmap& pixmap);

private slots:
    void OnAddressChanged(qint64 value);
    void OnFormatChanged(int value);
    void OnWidthChanged(int value);
    void OnHeightChanged(int value);
    void OnStrideChanged(int value);

private:
    QPixmap ReloadPixmap() const;

    Pica::DebugUtils::TextureInfo info;
};
