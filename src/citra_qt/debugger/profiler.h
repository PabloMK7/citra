// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QAbstractItemModel>
#include <QDockWidget>
#include <QTimer>
#include "ui_profiler.h"

#include "common/profiler_reporting.h"

class ProfilerModel : public QAbstractItemModel
{
    Q_OBJECT

public:
    ProfilerModel(QObject* parent);

    QVariant headerData(int section, Qt::Orientation orientation, int role = Qt::DisplayRole) const override;
    QModelIndex index(int row, int column, const QModelIndex& parent = QModelIndex()) const override;
    QModelIndex parent(const QModelIndex& child) const override;
    int columnCount(const QModelIndex& parent = QModelIndex()) const override;
    int rowCount(const QModelIndex& parent = QModelIndex()) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;

public slots:
    void updateProfilingInfo();

private:
    Common::Profiling::AggregatedFrameResult results;
};

class ProfilerWidget : public QDockWidget
{
    Q_OBJECT

public:
    ProfilerWidget(QWidget* parent = 0);

private slots:
    void setProfilingInfoUpdateEnabled(bool enable);

private:
    Ui::Profiler ui;
    ProfilerModel* model;

    QTimer update_timer;
};
