// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QHeaderView>
#include <QThreadPool>
#include <QVBoxLayout>

#include "game_list.h"
#include "game_list_p.h"

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
    item_model->setHeaderData(COLUMN_FILE_TYPE, Qt::Horizontal, "File type");
    item_model->setHeaderData(COLUMN_NAME, Qt::Horizontal, "Name");
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

void GameList::SaveInterfaceLayout(QSettings& settings)
{
    settings.beginGroup("UILayout");
    settings.setValue("gameListHeaderState", tree_view->header()->saveState());
    settings.endGroup();
}

void GameList::LoadInterfaceLayout(QSettings& settings)
{
    auto header = tree_view->header();
    settings.beginGroup("UILayout");
    header->restoreState(settings.value("gameListHeaderState").toByteArray());
    settings.endGroup();

    item_model->sort(header->sortIndicatorSection(), header->sortIndicatorOrder());
}

void GameListWorker::AddFstEntriesToGameList(const std::string& dir_path, bool deep_scan)
{
    const auto callback = [&](unsigned* num_entries_out,
                              const std::string& directory,
                              const std::string& virtual_name) -> bool {

        std::string physical_name = directory + DIR_SEP + virtual_name;

        if (stop_processing)
            return false; // Breaks the callback loop.

        if (deep_scan && FileUtil::IsDirectory(physical_name)) {
            AddFstEntriesToGameList(physical_name, true);
        } else {
            std::string filename_filename, filename_extension;
            Common::SplitPath(physical_name, nullptr, &filename_filename, &filename_extension);

            Loader::FileType guessed_filetype = Loader::GuessFromExtension(filename_extension);
            if (guessed_filetype == Loader::FileType::Unknown)
                return true;
            Loader::FileType filetype = Loader::IdentifyFile(physical_name);
            if (filetype == Loader::FileType::Unknown) {
                LOG_WARNING(Frontend, "File %s is of indeterminate type and is possibly corrupted.", physical_name.c_str());
                return true;
            }
            if (guessed_filetype != filetype) {
                LOG_WARNING(Frontend, "Filetype and extension of file %s do not match.", physical_name.c_str());
            }

            emit EntryReady({
                new GameListItem(QString::fromStdString(Loader::GetFileTypeString(filetype))),
                new GameListItemPath(QString::fromStdString(physical_name)),
                new GameListItemSize(FileUtil::GetSize(physical_name)),
            });
        }

        return true;
    };

    FileUtil::ForeachDirectoryEntry(nullptr, dir_path, callback);
}

void GameListWorker::run()
{
    stop_processing = false;
    AddFstEntriesToGameList(dir_path.toStdString(), deep_scan);
    emit Finished();
}

void GameListWorker::Cancel()
{
    disconnect(this, 0, 0, 0);
    stop_processing = true;
}
