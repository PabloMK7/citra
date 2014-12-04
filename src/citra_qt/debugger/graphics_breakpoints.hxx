// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2
// Refer to the license.txt file included.

#pragma once

#include <memory>

#include <QAbstractListModel>
#include <QDockWidget>

#include "video_core/debug_utils/debug_utils.h"

class QLabel;
class QPushButton;
class QTreeView;

class BreakPointModel;

class GraphicsBreakPointsWidget : public QDockWidget, Pica::DebugContext::BreakPointObserver {
    Q_OBJECT

    using Event = Pica::DebugContext::Event;

public:
    GraphicsBreakPointsWidget(std::shared_ptr<Pica::DebugContext> debug_context,
                              QWidget* parent = nullptr);

    void OnPicaBreakPointHit(Pica::DebugContext::Event event, void* data) override;
    void OnPicaResume() override;

public slots:
    void OnBreakPointHit(Pica::DebugContext::Event event, void* data);
    void OnResumeRequested();
    void OnResumed();
    void OnBreakpointSelectionChanged(const QModelIndex&);
    void OnToggleBreakpointEnabled();

signals:
    void Resumed();
    void BreakPointHit(Pica::DebugContext::Event event, void* data);
    void BreakPointsChanged(const QModelIndex& topLeft, const QModelIndex& bottomRight);

private:
    void UpdateToggleBreakpointButton(const QModelIndex& index);

    QLabel* status_text;
    QPushButton* resume_button;
    QPushButton* toggle_breakpoint_button;

    BreakPointModel* breakpoint_model;
    QTreeView* breakpoint_list;
};
