// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>

#include <QRunnable>
#include <QStandardItem>
#include <QString>

#include "citra_qt/util/util.h"
#include "common/string_util.h"


class GameListItem : public QStandardItem {

public:
    GameListItem(): QStandardItem() {}
    GameListItem(const QString& string): QStandardItem(string) {}
    virtual ~GameListItem() override {}
};


/**
 * A specialization of GameListItem for path values.
 * This class ensures that for every full path value it holds, a correct string representation
 * of just the filename (with no extension) will be displayed to the user.
 */
class GameListItemPath : public GameListItem {

public:
    static const int FullPathRole = Qt::UserRole + 1;

    GameListItemPath(): GameListItem() {}
    GameListItemPath(const QString& game_path): GameListItem()
    {
        setData(game_path, FullPathRole);
    }

    void setData(const QVariant& value, int role) override
    {
        // By specializing setData for FullPathRole, we can ensure that the two string
        // representations of the data are always accurate and in the correct format.
        if (role == FullPathRole) {
            std::string filename;
            Common::SplitPath(value.toString().toStdString(), nullptr, &filename, nullptr);
            GameListItem::setData(QString::fromStdString(filename), Qt::DisplayRole);
            GameListItem::setData(value, FullPathRole);
        } else {
            GameListItem::setData(value, role);
        }
    }
};


/**
 * A specialization of GameListItem for size values.
 * This class ensures that for every numerical size value it holds (in bytes), a correct
 * human-readable string representation will be displayed to the user.
 */
class GameListItemSize : public GameListItem {

public:
    static const int SizeRole = Qt::UserRole + 1;

    GameListItemSize(): GameListItem() {}
    GameListItemSize(const qulonglong size_bytes): GameListItem()
    {
        setData(size_bytes, SizeRole);
    }

    void setData(const QVariant& value, int role) override
    {
        // By specializing setData for SizeRole, we can ensure that the numerical and string
        // representations of the data are always accurate and in the correct format.
        if (role == SizeRole) {
            qulonglong size_bytes = value.toULongLong();
            GameListItem::setData(ReadableByteSize(size_bytes), Qt::DisplayRole);
            GameListItem::setData(value, SizeRole);
        } else {
            GameListItem::setData(value, role);
        }
    }

    /**
     * This operator is, in practice, only used by the TreeView sorting systems.
     * Override it so that it will correctly sort by numerical value instead of by string representation.
     */
    bool operator<(const QStandardItem& other) const override
    {
        return data(SizeRole).toULongLong() < other.data(SizeRole).toULongLong();
    }
};


/**
 * Asynchronous worker object for populating the game list.
 * Communicates with other threads through Qt's signal/slot system.
 */
class GameListWorker : public QObject, public QRunnable {
    Q_OBJECT

public:
    GameListWorker(QString dir_path, bool deep_scan):
            QObject(), QRunnable(), dir_path(dir_path), deep_scan(deep_scan) {}

public slots:
    /// Starts the processing of directory tree information.
    void run() override;
    /// Tells the worker that it should no longer continue processing. Thread-safe.
    void Cancel();

signals:
    /**
     * The `EntryReady` signal is emitted once an entry has been prepared and is ready
     * to be added to the game list.
     * @param entry_items a list with `QStandardItem`s that make up the columns of the new entry.
     */
    void EntryReady(QList<QStandardItem*> entry_items);
    void Finished();

private:
    QString dir_path;
    bool deep_scan;
    std::atomic_bool stop_processing;

    void AddFstEntriesToGameList(const std::string& dir_path, bool deep_scan);
};
