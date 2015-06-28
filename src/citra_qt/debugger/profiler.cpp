// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "profiler.h"

#include "common/profiler_reporting.h"

using namespace Common::Profiling;

static QVariant GetDataForColumn(int col, const AggregatedDuration& duration)
{
    static auto duration_to_float = [](Duration dur) -> float {
        using FloatMs = std::chrono::duration<float, std::chrono::milliseconds::period>;
        return std::chrono::duration_cast<FloatMs>(dur).count();
    };

    switch (col) {
    case 1: return duration_to_float(duration.avg);
    case 2: return duration_to_float(duration.min);
    case 3: return duration_to_float(duration.max);
    default: return QVariant();
    }
}

static const TimingCategoryInfo* GetCategoryInfo(int id)
{
    const auto& categories = GetProfilingManager().GetTimingCategoriesInfo();
    if ((size_t)id >= categories.size()) {
        return nullptr;
    } else {
        return &categories[id];
    }
}

ProfilerModel::ProfilerModel(QObject* parent) : QAbstractItemModel(parent)
{
    updateProfilingInfo();
    const auto& categories = GetProfilingManager().GetTimingCategoriesInfo();
    results.time_per_category.resize(categories.size());
}

QVariant ProfilerModel::headerData(int section, Qt::Orientation orientation, int role) const
{
    if (orientation == Qt::Horizontal && role == Qt::DisplayRole) {
        switch (section) {
        case 0: return tr("Category");
        case 1: return tr("Avg");
        case 2: return tr("Min");
        case 3: return tr("Max");
        }
    }

    return QVariant();
}

QModelIndex ProfilerModel::index(int row, int column, const QModelIndex& parent) const
{
    return createIndex(row, column);
}

QModelIndex ProfilerModel::parent(const QModelIndex& child) const
{
    return QModelIndex();
}

int ProfilerModel::columnCount(const QModelIndex& parent) const
{
    return 4;
}

int ProfilerModel::rowCount(const QModelIndex& parent) const
{
    if (parent.isValid()) {
        return 0;
    } else {
        return static_cast<int>(results.time_per_category.size() + 2);
    }
}

QVariant ProfilerModel::data(const QModelIndex& index, int role) const
{
    if (role == Qt::DisplayRole) {
        if (index.row() == 0) {
            if (index.column() == 0) {
                return tr("Frame");
            } else {
                return GetDataForColumn(index.column(), results.frame_time);
            }
        } else if (index.row() == 1) {
            if (index.column() == 0) {
                return tr("Frame (with swapping)");
            } else {
                return GetDataForColumn(index.column(), results.interframe_time);
            }
        } else {
            if (index.column() == 0) {
                const TimingCategoryInfo* info = GetCategoryInfo(index.row() - 2);
                return info != nullptr ? QString(info->name) : QVariant();
            } else {
                if (index.row() - 2 < (int)results.time_per_category.size()) {
                    return GetDataForColumn(index.column(), results.time_per_category[index.row() - 2]);
                } else {
                    return QVariant();
                }
            }
        }
    }

    return QVariant();
}

void ProfilerModel::updateProfilingInfo()
{
    results = GetTimingResultsAggregator()->GetAggregatedResults();
    emit dataChanged(createIndex(0, 1), createIndex(rowCount() - 1, 3));
}

ProfilerWidget::ProfilerWidget(QWidget* parent) : QDockWidget(parent)
{
    ui.setupUi(this);

    model = new ProfilerModel(this);
    ui.treeView->setModel(model);

    connect(this, SIGNAL(visibilityChanged(bool)), SLOT(setProfilingInfoUpdateEnabled(bool)));
    connect(&update_timer, SIGNAL(timeout()), model, SLOT(updateProfilingInfo()));
}

void ProfilerWidget::setProfilingInfoUpdateEnabled(bool enable)
{
    if (enable) {
        update_timer.start(100);
        model->updateProfilingInfo();
    } else {
        update_timer.stop();
    }
}
