// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <QModelIndex>
#include <QSettings>
#include <QStandardItem>
#include <QStandardItemModel>
#include <QString>
#include <QTreeView>
#include <QWidget>

class GameListWorker;

class GameList : public QWidget {
    Q_OBJECT

public:
    enum {
        COLUMN_NAME,
        COLUMN_FILE_TYPE,
        COLUMN_SIZE,
        COLUMN_COUNT, // Number of columns
    };

    GameList(QWidget* parent = nullptr);
    ~GameList() override;

    void PopulateAsync(const QString& dir_path, bool deep_scan);

    void SaveInterfaceLayout();
    void LoadInterfaceLayout();

public slots:
    void AddEntry(QList<QStandardItem*> entry_items);

private slots:
    void ValidateEntry(const QModelIndex& item);
    void DonePopulating();

signals:
    void GameChosen(QString game_path);
    void ShouldCancelWorker();

private:
    QTreeView* tree_view = nullptr;
    QStandardItemModel* item_model = nullptr;
    GameListWorker* current_worker = nullptr;
};
