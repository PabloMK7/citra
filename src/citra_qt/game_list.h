// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include <QString>
#include <QWidget>
#include "common/common_types.h"
#include "ui_settings.h"

class GameListWorker;
class GameListDir;
class GMainWindow;
class QFileSystemWatcher;
class QHBoxLayout;
class QLabel;
class QLineEdit;
template <typename>
class QList;
class QModelIndex;
class QStandardItem;
class QStandardItemModel;
class QTreeView;
class QToolButton;
class QVBoxLayout;

enum class GameListOpenTarget { SAVE_DATA = 0, APPLICATION = 1, UPDATE_DATA = 2 };

class GameList : public QWidget {
    Q_OBJECT

public:
    enum {
        COLUMN_NAME,
        COLUMN_COMPATIBILITY,
        COLUMN_REGION,
        COLUMN_FILE_TYPE,
        COLUMN_SIZE,
        COLUMN_COUNT, // Number of columns
    };

    class SearchField : public QWidget {
    public:
        explicit SearchField(GameList* parent = nullptr);

        void setFilterResult(int visible, int total);
        void clear();
        void setFocus();

        int visible;
        int total;

    private:
        class KeyReleaseEater : public QObject {
        public:
            explicit KeyReleaseEater(GameList* gamelist);

        private:
            GameList* gamelist = nullptr;
            QString edit_filter_text_old;

        protected:
            bool eventFilter(QObject* obj, QEvent* event) override;
        };
        QHBoxLayout* layout_filter = nullptr;
        QTreeView* tree_view = nullptr;
        QLabel* label_filter = nullptr;
        QLineEdit* edit_filter = nullptr;
        QLabel* label_filter_result = nullptr;
        QToolButton* button_filter_close = nullptr;
    };

    explicit GameList(GMainWindow* parent = nullptr);
    ~GameList() override;

    QString getLastFilterResultItem();
    void clearFilter();
    void setFilterFocus();
    void setFilterVisible(bool visibility);
    bool isEmpty();

    void LoadCompatibilityList();
    void PopulateAsync(QList<UISettings::GameDir>& game_dirs);

    void SaveInterfaceLayout();
    void LoadInterfaceLayout();

    QStandardItemModel* GetModel() const;

    static const QStringList supported_file_extensions;

signals:
    void GameChosen(QString game_path);
    void ShouldCancelWorker();
    void OpenFolderRequested(u64 program_id, GameListOpenTarget target);
    void NavigateToGamedbEntryRequested(
        u64 program_id,
        std::unordered_map<std::string, std::pair<QString, QString>>& compatibility_list);
    void OpenDirectory(QString directory);
    void AddDirectory();
    void ShowList(bool show);

private slots:
    void onItemExpanded(const QModelIndex& item);
    void onTextChanged(const QString& newText);
    void onFilterCloseClicked();
    void onUpdateThemedIcons();

private:
    void AddDirEntry(GameListDir* entry_items);
    void AddEntry(const QList<QStandardItem*>& entry_items, GameListDir* parent);
    void ValidateEntry(const QModelIndex& item);
    void DonePopulating(QStringList watch_list);

    void RefreshGameDirectory();
    bool containsAllWords(QString haystack, QString userinput);

    void PopupContextMenu(const QPoint& menu_location);
    void AddGamePopup(QMenu& context_menu, u64 program_id);
    void AddCustomDirPopup(QMenu& context_menu, QModelIndex selected);
    void AddPermDirPopup(QMenu& context_menu, QModelIndex selected);

    SearchField* search_field;
    GMainWindow* main_window = nullptr;
    QVBoxLayout* layout = nullptr;
    QTreeView* tree_view = nullptr;
    QStandardItemModel* item_model = nullptr;
    GameListWorker* current_worker = nullptr;
    QFileSystemWatcher* watcher = nullptr;
    std::unordered_map<std::string, std::pair<QString, QString>> compatibility_list;
};

Q_DECLARE_METATYPE(GameListOpenTarget);

class GameListPlaceholder : public QWidget {
    Q_OBJECT
public:
    explicit GameListPlaceholder(GMainWindow* parent = nullptr);
    ~GameListPlaceholder();

signals:
    void AddDirectory();

private slots:
    void onUpdateThemedIcons();

protected:
    void mouseDoubleClickEvent(QMouseEvent* event) override;

private:
    GMainWindow* main_window = nullptr;
    QVBoxLayout* layout = nullptr;
    QLabel* image = nullptr;
    QLabel* text = nullptr;
};
