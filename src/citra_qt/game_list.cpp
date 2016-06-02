// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QHeaderView>
#include <QThreadPool>
#include <QVBoxLayout>

#include "game_list.h"
#include "game_list_p.h"
#include "ui_settings.h"

#include "core/loader/loader.h"

#include "common/common_paths.h"
#include "common/logging/log.h"
#include "common/string_util.h"

GameList::GameList(QWidget* parent)
{
    QVBoxLayout* layout = new QVBoxLayout;

    tree_view = new QTreeView;
    item_model = new QStandardItemModel(tree_view);
    tree_view->setModel(item_model);

    tree_view->setAlternatingRowColors(true);
    tree_view->setSelectionMode(QHeaderView::SingleSelection);
    tree_view->setSelectionBehavior(QHeaderView::SelectRows);
    tree_view->setVerticalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setHorizontalScrollMode(QHeaderView::ScrollPerPixel);
    tree_view->setSortingEnabled(true);
    tree_view->setEditTriggers(QHeaderView::NoEditTriggers);
    tree_view->setUniformRowHeights(true);

    item_model->insertColumns(0, COLUMN_COUNT);
    item_model->setHeaderData(COLUMN_NAME, Qt::Horizontal, "Name");
    item_model->setHeaderData(COLUMN_FILE_TYPE, Qt::Horizontal, "File type");
    item_model->setHeaderData(COLUMN_SIZE, Qt::Horizontal, "Size");

    connect(tree_view, SIGNAL(activated(const QModelIndex&)), this, SLOT(ValidateEntry(const QModelIndex&)));

    // We must register all custom types with the Qt Automoc system so that we are able to use it with
    // signals/slots. In this case, QList falls under the umbrells of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    layout->addWidget(tree_view);
    setLayout(layout);
}

GameList::~GameList()
{
    emit ShouldCancelWorker();
}

void GameList::AddEntry(QList<QStandardItem*> entry_items)
{
    item_model->invisibleRootItem()->appendRow(entry_items);
}

void GameList::ValidateEntry(const QModelIndex& item)
{
    // We don't care about the individual QStandardItem that was selected, but its row.
    int row = item_model->itemFromIndex(item)->row();
    QStandardItem* child_file = item_model->invisibleRootItem()->child(row, COLUMN_NAME);
    QString file_path = child_file->data(GameListItemPath::FullPathRole).toString();

    if (file_path.isEmpty())
        return;
    std::string std_file_path(file_path.toStdString());
    if (!FileUtil::Exists(std_file_path) || FileUtil::IsDirectory(std_file_path))
        return;
    emit GameChosen(file_path);
}

void GameList::DonePopulating()
{
    tree_view->setEnabled(true);
}

void GameList::PopulateAsync(const QString& dir_path, bool deep_scan)
{
    if (!FileUtil::Exists(dir_path.toStdString()) || !FileUtil::IsDirectory(dir_path.toStdString())) {
        LOG_ERROR(Frontend, "Could not find game list folder at %s", dir_path.toLocal8Bit().data());
        return;
    }

    tree_view->setEnabled(false);
    // Delete any rows that might already exist if we're repopulating
    item_model->removeRows(0, item_model->rowCount());

    emit ShouldCancelWorker();
    GameListWorker* worker = new GameListWorker(dir_path, deep_scan);

    connect(worker, SIGNAL(EntryReady(QList<QStandardItem*>)), this, SLOT(AddEntry(QList<QStandardItem*>)), Qt::QueuedConnection);
    connect(worker, SIGNAL(Finished()), this, SLOT(DonePopulating()), Qt::QueuedConnection);
    // Use DirectConnection here because worker->Cancel() is thread-safe and we want it to cancel without delay.
    connect(this, SIGNAL(ShouldCancelWorker()), worker, SLOT(Cancel()), Qt::DirectConnection);

    QThreadPool::globalInstance()->start(worker);
    current_worker = std::move(worker);
}

void GameList::SaveInterfaceLayout()
{
    UISettings::values.gamelist_header_state = tree_view->header()->saveState();
}

void GameList::LoadInterfaceLayout()
{
    auto header = tree_view->header();
    if (!header->restoreState(UISettings::values.gamelist_header_state)) {
        // We are using the name column to display icons and titles
        // so make it as large as possible as default.
        header->resizeSection(COLUMN_NAME, header->width());
    }

    item_model->sort(header->sortIndicatorSection(), header->sortIndicatorOrder());
}

void GameListWorker::AddFstEntriesToGameList(const std::string& dir_path, unsigned int recursion)
{
    const auto callback = [&](unsigned* num_entries_out,
                              const std::string& directory,
                              const std::string& virtual_name,
                              unsigned int recursion) -> bool {

        std::string physical_name = directory + DIR_SEP + virtual_name;

        if (stop_processing)
            return false; // Breaks the callback loop.

        if (!FileUtil::IsDirectory(physical_name)) {
            std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(physical_name);
            if (!loader)
                return true;

            std::vector<u8> smdh;
            loader->ReadIcon(smdh);

            emit EntryReady({
                new GameListItemPath(QString::fromStdString(physical_name), smdh),
                new GameListItem(QString::fromStdString(Loader::GetFileTypeString(loader->GetFileType()))),
                new GameListItemSize(FileUtil::GetSize(physical_name)),
            });
        } else if (recursion > 0) {
            AddFstEntriesToGameList(physical_name, recursion - 1);
        }

        return true;
    };

    FileUtil::ForeachDirectoryEntry(nullptr, dir_path, callback);
}

void GameListWorker::run()
{
    stop_processing = false;
    AddFstEntriesToGameList(dir_path.toStdString(), deep_scan ? 256 : 0);
    emit Finished();
}

void GameListWorker::Cancel()
{
    disconnect(this, 0, 0, 0);
    stop_processing = true;
}
