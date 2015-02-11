// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QAbstractListModel>

#include "graphics_breakpoint_observer.h"

#include "nihstro/parser_shbin.h"

class GraphicsVertexShaderModel : public QAbstractItemModel {
    Q_OBJECT

public:
    GraphicsVertexShaderModel(QObject* parent);

    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;

public slots:
    void OnUpdate();

private:
    nihstro::ShaderInfo info;
};

class GraphicsVertexShaderWidget : public BreakPointObserverDock {
    Q_OBJECT

    using Event = Pica::DebugContext::Event;

public:
    GraphicsVertexShaderWidget(std::shared_ptr<Pica::DebugContext> debug_context,
                               QWidget* parent = nullptr);

private slots:
    void OnBreakPointHit(Pica::DebugContext::Event event, void* data) override;
    void OnResumed() override;

signals:
    void Update();

private:

};
