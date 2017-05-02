// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QFileInfo>
#include <QHeaderView>
#include <QKeyEvent>
#include <QMenu>
#include <QThreadPool>
#include "common/common_paths.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/loader/loader.h"
#include "game_list.h"
#include "game_list_p.h"
#include "ui_settings.h"

GameList::SearchField::KeyReleaseEater::KeyReleaseEater(GameList* gamelist) {
    this->gamelist = gamelist;
    edit_filter_text_old = "";
}

// EventFilter in order to process systemkeys while editing the searchfield
bool GameList::SearchField::KeyReleaseEater::eventFilter(QObject* obj, QEvent* event) {
    // If it isn't a KeyRelease event then continue with standard event processing
    if (event->type() != QEvent::KeyRelease)
        return QObject::eventFilter(obj, event);

    QKeyEvent* keyEvent = static_cast<QKeyEvent*>(event);
    int rowCount = gamelist->tree_view->model()->rowCount();
    QString edit_filter_text = gamelist->search_field->edit_filter->text().toLower();

    // If the searchfield's text hasn't changed special function keys get checked
    // If no function key changes the searchfield's text the filter doesn't need to get reloaded
    if (edit_filter_text == edit_filter_text_old) {
        switch (keyEvent->key()) {
        // Escape: Resets the searchfield
        case Qt::Key_Escape: {
            if (edit_filter_text_old.isEmpty()) {
                return QObject::eventFilter(obj, event);
            } else {
                gamelist->search_field->edit_filter->clear();
                edit_filter_text = "";
            }
            break;
        }
        // Return and Enter
        // If the enter key gets pressed first checks how many and which entry is visible
        // If there is only one result launch this game
        case Qt::Key_Return:
        case Qt::Key_Enter: {
            QStandardItemModel* item_model = new QStandardItemModel(gamelist->tree_view);
            QModelIndex root_index = item_model->invisibleRootItem()->index();
            QStandardItem* child_file;
            QString file_path;
            int resultCount = 0;
            for (int i = 0; i < rowCount; ++i) {
                if (!gamelist->tree_view->isRowHidden(i, root_index)) {
                    ++resultCount;
                    child_file = gamelist->item_model->item(i, 0);
                    file_path = child_file->data(GameListItemPath::FullPathRole).toString();
                }
            }
            if (resultCount == 1) {
                // To avoid loading error dialog loops while confirming them using enter
                // Also users usually want to run a diffrent game after closing one
                gamelist->search_field->edit_filter->setText("");
                edit_filter_text = "";
                emit gamelist->GameChosen(file_path);
            } else {
                return QObject::eventFilter(obj, event);
            }
            break;
        }
        default:
            return QObject::eventFilter(obj, event);
        }
    }
    edit_filter_text_old = edit_filter_text;
    return QObject::eventFilter(obj, event);
}

void GameList::SearchField::setFilterResult(int visible, int total) {
    QString result_of_text = tr("of");
    QString result_text;
    if (total == 1) {
        result_text = tr("result");
    } else {
        result_text = tr("results");
    }
    label_filter_result->setText(
        QString("%1 %2 %3 %4").arg(visible).arg(result_of_text).arg(total).arg(result_text));
}

void GameList::SearchField::clear() {
    edit_filter->setText("");
}

void GameList::SearchField::setFocus() {
    if (edit_filter->isVisible()) {
        edit_filter->setFocus();
    }
}

GameList::SearchField::SearchField(GameList* parent) : QWidget{parent} {
    KeyReleaseEater* keyReleaseEater = new KeyReleaseEater(parent);
    layout_filter = new QHBoxLayout;
    layout_filter->setMargin(8);
    label_filter = new QLabel;
    label_filter->setText(tr("Filter:"));
    edit_filter = new QLineEdit;
    edit_filter->setText("");
    edit_filter->setPlaceholderText(tr("Enter pattern to filter"));
    edit_filter->installEventFilter(keyReleaseEater);
    edit_filter->setClearButtonEnabled(true);
    connect(edit_filter, SIGNAL(textChanged(const QString&)), parent,
            SLOT(onTextChanged(const QString&)));
    label_filter_result = new QLabel;
    button_filter_close = new QToolButton(this);
    button_filter_close->setText("X");
    button_filter_close->setCursor(Qt::ArrowCursor);
    button_filter_close->setStyleSheet("QToolButton{ border: none; padding: 0px; color: "
                                       "#000000; font-weight: bold; background: #F0F0F0; }"
                                       "QToolButton:hover{ border: none; padding: 0px; color: "
                                       "#EEEEEE; font-weight: bold; background: #E81123}");
    connect(button_filter_close, SIGNAL(clicked()), parent, SLOT(onFilterCloseClicked()));
    layout_filter->setSpacing(10);
    layout_filter->addWidget(label_filter);
    layout_filter->addWidget(edit_filter);
    layout_filter->addWidget(label_filter_result);
    layout_filter->addWidget(button_filter_close);
    setLayout(layout_filter);
}

/**
 * Checks if all words separated by spaces are contained in another string
 * This offers a word order insensitive search function
 *
 * @param String that gets checked if it contains all words of the userinput string
 * @param String containing all words getting checked
 * @return true if the haystack contains all words of userinput
 */
bool GameList::containsAllWords(QString haystack, QString userinput) {
    QStringList userinput_split = userinput.split(" ", QString::SplitBehavior::SkipEmptyParts);
    return std::all_of(userinput_split.begin(), userinput_split.end(),
                       [haystack](QString s) { return haystack.contains(s); });
}

// Event in order to filter the gamelist after editing the searchfield
void GameList::onTextChanged(const QString& newText) {
    int rowCount = tree_view->model()->rowCount();
    QString edit_filter_text = newText.toLower();

    QModelIndex root_index = item_model->invisibleRootItem()->index();

    // If the searchfield is empty every item is visible
    // Otherwise the filter gets applied
    if (edit_filter_text.isEmpty()) {
        for (int i = 0; i < rowCount; ++i) {
            tree_view->setRowHidden(i, root_index, false);
        }
        search_field->setFilterResult(rowCount, rowCount);
    } else {
        QStandardItem* child_file;
        QString file_path, file_name, file_title, file_programmid;
        int result_count = 0;
        for (int i = 0; i < rowCount; ++i) {
            child_file = item_model->item(i, 0);
            file_path = child_file->data(GameListItemPath::FullPathRole).toString().toLower();
            file_name = file_path.mid(file_path.lastIndexOf("/") + 1);
            file_title = child_file->data(GameListItemPath::TitleRole).toString().toLower();
            file_programmid =
                child_file->data(GameListItemPath::ProgramIdRole).toString().toLower();

            // Only items which filename in combination with its title contains all words
            // that are in the searchfiel will be visible in the gamelist
            // The search is case insensitive because of toLower()
            // I decided not to use Qt::CaseInsensitive in containsAllWords to prevent
            // multiple conversions of edit_filter_text for each game in the gamelist
            if (containsAllWords(file_name.append(" ").append(file_title), edit_filter_text) ||
                (file_programmid.count() == 16 && edit_filter_text.contains(file_programmid))) {
                tree_view->setRowHidden(i, root_index, false);
                ++result_count;
            } else {
                tree_view->setRowHidden(i, root_index, true);
            }
            search_field->setFilterResult(result_count, rowCount);
        }
    }
}

void GameList::onFilterCloseClicked() {
    main_window->filterBarSetChecked(false);
}

GameList::GameList(GMainWindow* parent) : QWidget{parent} {
    this->main_window = parent;
    layout = new QVBoxLayout;
    tree_view = new QTreeView;
    search_field = new SearchField(this);
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
    tree_view->setContextMenuPolicy(Qt::CustomContextMenu);

    item_model->insertColumns(0, COLUMN_COUNT);
    item_model->setHeaderData(COLUMN_NAME, Qt::Horizontal, "Name");
    item_model->setHeaderData(COLUMN_FILE_TYPE, Qt::Horizontal, "File type");
    item_model->setHeaderData(COLUMN_SIZE, Qt::Horizontal, "Size");

    connect(tree_view, &QTreeView::activated, this, &GameList::ValidateEntry);
    connect(tree_view, &QTreeView::customContextMenuRequested, this, &GameList::PopupContextMenu);
    connect(&watcher, &QFileSystemWatcher::directoryChanged, this, &GameList::RefreshGameDirectory);

    // We must register all custom types with the Qt Automoc system so that we are able to use it
    // with signals/slots. In this case, QList falls under the umbrells of custom types.
    qRegisterMetaType<QList<QStandardItem*>>("QList<QStandardItem*>");

    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(0);
    layout->addWidget(tree_view);
    layout->addWidget(search_field);
    setLayout(layout);
}

GameList::~GameList() {
    emit ShouldCancelWorker();
}

void GameList::setFilterFocus() {
    search_field->setFocus();
}

void GameList::setFilterVisible(bool visablility) {
    search_field->setVisible(visablility);
}

void GameList::clearFilter() {
    search_field->clear();
}

void GameList::AddEntry(const QList<QStandardItem*>& entry_items) {
    item_model->invisibleRootItem()->appendRow(entry_items);
}

void GameList::ValidateEntry(const QModelIndex& item) {
    // We don't care about the individual QStandardItem that was selected, but its row.
    int row = item_model->itemFromIndex(item)->row();
    QStandardItem* child_file = item_model->invisibleRootItem()->child(row, COLUMN_NAME);
    QString file_path = child_file->data(GameListItemPath::FullPathRole).toString();

    if (file_path.isEmpty())
        return;
    std::string std_file_path(file_path.toStdString());
    if (!FileUtil::Exists(std_file_path) || FileUtil::IsDirectory(std_file_path))
        return;
    // Users usually want to run a diffrent game after closing one
    search_field->clear();
    emit GameChosen(file_path);
}

void GameList::DonePopulating() {
    tree_view->setEnabled(true);
    int rowCount = tree_view->model()->rowCount();
    search_field->setFilterResult(rowCount, rowCount);
    search_field->setFocus();
}

void GameList::PopupContextMenu(const QPoint& menu_location) {
    QModelIndex item = tree_view->indexAt(menu_location);
    if (!item.isValid())
        return;

    int row = item_model->itemFromIndex(item)->row();
    QStandardItem* child_file = item_model->invisibleRootItem()->child(row, COLUMN_NAME);
    u64 program_id = child_file->data(GameListItemPath::ProgramIdRole).toULongLong();

    QMenu context_menu;
    QAction* open_save_location = context_menu.addAction(tr("Open Save Data Location"));
    open_save_location->setEnabled(program_id != 0);
    connect(open_save_location, &QAction::triggered,
            [&]() { emit OpenSaveFolderRequested(program_id); });
    context_menu.exec(tree_view->viewport()->mapToGlobal(menu_location));
}

void GameList::PopulateAsync(const QString& dir_path, bool deep_scan) {
    if (!FileUtil::Exists(dir_path.toStdString()) ||
        !FileUtil::IsDirectory(dir_path.toStdString())) {
        LOG_ERROR(Frontend, "Could not find game list folder at %s", dir_path.toLocal8Bit().data());
        search_field->setFilterResult(0, 0);
        search_field->setFocus();
        return;
    }

    tree_view->setEnabled(false);
    // Delete any rows that might already exist if we're repopulating
    item_model->removeRows(0, item_model->rowCount());

    emit ShouldCancelWorker();

    auto watch_dirs = watcher.directories();
    if (!watch_dirs.isEmpty()) {
        watcher.removePaths(watch_dirs);
    }
    UpdateWatcherList(dir_path.toStdString(), deep_scan ? 256 : 0);
    GameListWorker* worker = new GameListWorker(dir_path, deep_scan);

    connect(worker, &GameListWorker::EntryReady, this, &GameList::AddEntry, Qt::QueuedConnection);
    connect(worker, &GameListWorker::Finished, this, &GameList::DonePopulating,
            Qt::QueuedConnection);
    // Use DirectConnection here because worker->Cancel() is thread-safe and we want it to cancel
    // without delay.
    connect(this, &GameList::ShouldCancelWorker, worker, &GameListWorker::Cancel,
            Qt::DirectConnection);

    QThreadPool::globalInstance()->start(worker);
    current_worker = std::move(worker);
}

void GameList::SaveInterfaceLayout() {
    UISettings::values.gamelist_header_state = tree_view->header()->saveState();
}

void GameList::LoadInterfaceLayout() {
    auto header = tree_view->header();
    if (!header->restoreState(UISettings::values.gamelist_header_state)) {
        // We are using the name column to display icons and titles
        // so make it as large as possible as default.
        header->resizeSection(COLUMN_NAME, header->width());
    }

    item_model->sort(header->sortIndicatorSection(), header->sortIndicatorOrder());
}

const QStringList GameList::supported_file_extensions = {"3ds", "3dsx", "elf", "axf",
                                                         "cci", "cxi",  "app"};

static bool HasSupportedFileExtension(const std::string& file_name) {
    QFileInfo file = QFileInfo(file_name.c_str());
    return GameList::supported_file_extensions.contains(file.suffix(), Qt::CaseInsensitive);
}

void GameList::RefreshGameDirectory() {
    if (!UISettings::values.gamedir.isEmpty() && current_worker != nullptr) {
        LOG_INFO(Frontend, "Change detected in the games directory. Reloading game list.");
        search_field->clear();
        PopulateAsync(UISettings::values.gamedir, UISettings::values.gamedir_deepscan);
    }
}

/**
 * Adds the game list folder to the QFileSystemWatcher to check for updates.
 *
 * The file watcher will fire off an update to the game list when a change is detected in the game
 * list folder.
 *
 * Notice: This method is run on the UI thread because QFileSystemWatcher is not thread safe and
 * this function is fast enough to not stall the UI thread. If performance is an issue, it should
 * be moved to another thread and properly locked to prevent concurrency issues.
 *
 * @param dir folder to check for changes in
 * @param recursion 0 if recursion is disabled. Any positive number passed to this will add each
 *        directory recursively to the watcher and will update the file list if any of the folders
 *        change. The number determines how deep the recursion should traverse.
 */
void GameList::UpdateWatcherList(const std::string& dir, unsigned int recursion) {
    const auto callback = [this, recursion](unsigned* num_entries_out, const std::string& directory,
                                            const std::string& virtual_name) -> bool {
        std::string physical_name = directory + DIR_SEP + virtual_name;

        if (FileUtil::IsDirectory(physical_name)) {
            UpdateWatcherList(physical_name, recursion - 1);
        }
        return true;
    };

    watcher.addPath(QString::fromStdString(dir));
    if (recursion > 0) {
        FileUtil::ForeachDirectoryEntry(nullptr, dir, callback);
    }
}

void GameListWorker::AddFstEntriesToGameList(const std::string& dir_path, unsigned int recursion) {
    const auto callback = [this, recursion](unsigned* num_entries_out, const std::string& directory,
                                            const std::string& virtual_name) -> bool {
        std::string physical_name = directory + DIR_SEP + virtual_name;

        if (stop_processing)
            return false; // Breaks the callback loop.

        if (!FileUtil::IsDirectory(physical_name) && HasSupportedFileExtension(physical_name)) {
            std::unique_ptr<Loader::AppLoader> loader = Loader::GetLoader(physical_name);
            if (!loader)
                return true;

            std::vector<u8> smdh;
            loader->ReadIcon(smdh);

            u64 program_id = 0;
            loader->ReadProgramId(program_id);

            emit EntryReady({
                new GameListItemPath(QString::fromStdString(physical_name), smdh, program_id),
                new GameListItem(
                    QString::fromStdString(Loader::GetFileTypeString(loader->GetFileType()))),
                new GameListItemSize(FileUtil::GetSize(physical_name)),
            });
        } else if (recursion > 0) {
            AddFstEntriesToGameList(physical_name, recursion - 1);
        }

        return true;
    };

    FileUtil::ForeachDirectoryEntry(nullptr, dir_path, callback);
}

void GameListWorker::run() {
    stop_processing = false;
    AddFstEntriesToGameList(dir_path.toStdString(), deep_scan ? 256 : 0);
    emit Finished();
}

void GameListWorker::Cancel() {
    this->disconnect();
    stop_processing = true;
}
