// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QAbstractItemModel>
#include <QDockWidget>
#include <QTimer>
#include "common/microprofile.h"
#include "common/profiler_reporting.h"
#include "ui_profiler.h"

class ProfilerModel : public QAbstractItemModel {
    Q_OBJECT

public:
    ProfilerModel(QObject* parent);

    QVariant headerData(int section, Qt::Orientation orientation,
                        int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column,
                      const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

public slots:
    void updateProfilingInfo();

private:
    Common::Profiling::AggregatedFrameResult results;
};

class ProfilerWidget : public QDockWidget {
    Q_OBJECT

public:
    ProfilerWidget(QWidget* parent = nullptr);

private slots:
    void setProfilingInfoUpdateEnabled(bool enable);

private:
    Ui::Profiler ui;
    ProfilerModel* model;

    QTimer update_timer;
};

class MicroProfileDialog : public QWidget {
    Q_OBJECT

public:
    MicroProfileDialog(QWidget* parent = nullptr);

    /// Returns a QAction that can be used to toggle visibility of this dialog.
    QAction* toggleViewAction();

protected:
    void showEvent(QShowEvent* ev) override;
    void hideEvent(QHideEvent* ev) override;

private:
    QAction* toggle_view_action = nullptr;
};
