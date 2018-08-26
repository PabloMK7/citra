// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <atomic>
#include <map>
#include <unordered_map>
#include <utility>
#include <QCoreApplication>
#include <QFileInfo>
#include <QImage>
#include <QObject>
#include <QPainter>
#include <QRunnable>
#include <QStandardItem>
#include <QString>
#include <QWidget>
#include "citra_qt/ui_settings.h"
#include "citra_qt/util/util.h"
#include "common/file_util.h"
#include "common/logging/log.h"
#include "common/string_util.h"
#include "core/loader/smdh.h"

enum class GameListItemType {
    Game = QStandardItem::UserType + 1,
    CustomDir = QStandardItem::UserType + 2,
    InstalledDir = QStandardItem::UserType + 3,
    SystemDir = QStandardItem::UserType + 4,
    AddDir = QStandardItem::UserType + 5
};

Q_DECLARE_METATYPE(GameListItemType);

/**
 * Gets the game icon from SMDH data.
 * @param smdh SMDH data
 * @param large If true, returns large icon (48x48), otherwise returns small icon (24x24)
 * @return QPixmap game icon
 */
static QPixmap GetQPixmapFromSMDH(const Loader::SMDH& smdh, bool large) {
    std::vector<u16> icon_data = smdh.GetIcon(large);
    const uchar* data = reinterpret_cast<const uchar*>(icon_data.data());
    int size = large ? 48 : 24;
    QImage icon(data, size, size, QImage::Format::Format_RGB16);
    return QPixmap::fromImage(icon);
}

/**
 * Gets the default icon (for games without valid SMDH)
 * @param large If true, returns large icon (48x48), otherwise returns small icon (24x24)
 * @return QPixmap default icon
 */
static QPixmap GetDefaultIcon(bool large) {
    int size = large ? 48 : 24;
    QPixmap icon(size, size);
    icon.fill(Qt::transparent);
    return icon;
}

/**
 * Creates a circle pixmap from a specified color
 * @param color The color the pixmap shall have
 * @return QPixmap circle pixmap
 */
static QPixmap CreateCirclePixmapFromColor(const QColor& color) {
    QPixmap circle_pixmap(16, 16);
    circle_pixmap.fill(Qt::transparent);

    QPainter painter(&circle_pixmap);
    painter.setPen(color);
    painter.setBrush(color);
    painter.drawEllipse(0, 0, 15, 15);

    return circle_pixmap;
}

/**
 * Gets the short game title from SMDH data.
 * @param smdh SMDH data
 * @param language title language
 * @return QString short title
 */
static QString GetQStringShortTitleFromSMDH(const Loader::SMDH& smdh,
                                            Loader::SMDH::TitleLanguage language) {
    return QString::fromUtf16(smdh.GetShortTitle(language).data());
}

/**
 * Gets the game region from SMDH data.
 * @param smdh SMDH data
 * @return QString region
 */
static QString GetRegionFromSMDH(const Loader::SMDH& smdh) {
    const Loader::SMDH::GameRegion region = smdh.GetRegion();

    switch (region) {
    case Loader::SMDH::GameRegion::Invalid:
        return QObject::tr("Invalid region");
    case Loader::SMDH::GameRegion::Japan:
        return QObject::tr("Japan");
    case Loader::SMDH::GameRegion::NorthAmerica:
        return QObject::tr("North America");
    case Loader::SMDH::GameRegion::Europe:
        return QObject::tr("Europe");
    case Loader::SMDH::GameRegion::Australia:
        return QObject::tr("Australia");
    case Loader::SMDH::GameRegion::China:
        return QObject::tr("China");
    case Loader::SMDH::GameRegion::Korea:
        return QObject::tr("Korea");
    case Loader::SMDH::GameRegion::Taiwan:
        return QObject::tr("Taiwan");
    case Loader::SMDH::GameRegion::RegionFree:
        return QObject::tr("Region free");
    default:
        return QObject::tr("Invalid Region");
    }
}

struct CompatStatus {
    QString color;
    const char* text;
    const char* tooltip;
};

// When this is put in a class, MSVS builds crash when closing Citra
// clang-format off
const static inline std::map<QString, CompatStatus> status_data = {
{ "0", { "#5c93ed", QT_TRANSLATE_NOOP("GameList", "Perfect"),    QT_TRANSLATE_NOOP("GameList", "Game functions flawless with no audio or graphical glitches, all tested functionality works as intended without\nany workarounds needed.") } },
{ "1", { "#47d35c", QT_TRANSLATE_NOOP("GameList", "Great"),      QT_TRANSLATE_NOOP("GameList", "Game functions with minor graphical or audio glitches and is playable from start to finish. May require some\nworkarounds.") } },
{ "2", { "#94b242", QT_TRANSLATE_NOOP("GameList", "Okay"),       QT_TRANSLATE_NOOP("GameList", "Game functions with major graphical or audio glitches, but game is playable from start to finish with\nworkarounds.") } },
{ "3", { "#f2d624", QT_TRANSLATE_NOOP("GameList", "Bad"),        QT_TRANSLATE_NOOP("GameList", "Game functions, but with major graphical or audio glitches. Unable to progress in specific areas due to glitches\neven with workarounds.") } },
{ "4", { "#FF0000", QT_TRANSLATE_NOOP("GameList", "Intro/Menu"), QT_TRANSLATE_NOOP("GameList", "Game is completely unplayable due to major graphical or audio glitches. Unable to progress past the Start\nScreen.") } },
{ "5", { "#828282", QT_TRANSLATE_NOOP("GameList", "Won't Boot"), QT_TRANSLATE_NOOP("GameList", "The game crashes when attempting to startup.") } },
{ "99",{ "#000000", QT_TRANSLATE_NOOP("GameList", "Not Tested"), QT_TRANSLATE_NOOP("GameList", "The game has not yet been tested.") } }, };
// clang-format on

class GameListItem : public QStandardItem {
public:
    // used to access type from item index
    static const int TypeRole = Qt::UserRole + 1;
    static const int SortRole = Qt::UserRole + 2;
    GameListItem() = default;
    explicit GameListItem(const QString& string) : QStandardItem(string) {
        setData(string, SortRole);
    }
};

/**
 * A specialization of GameListItem for path values.
 * This class ensures that for every full path value it holds, a correct string representation
 * of just the filename (with no extension) will be displayed to the user.
 * If this class receives valid SMDH data, it will also display game icons and titles.
 */
class GameListItemPath : public GameListItem {
public:
    static const int TitleRole = SortRole;
    static const int FullPathRole = SortRole + 1;
    static const int ProgramIdRole = SortRole + 2;

    GameListItemPath() = default;
    GameListItemPath(const QString& game_path, const std::vector<u8>& smdh_data, u64 program_id) {
        setData(type(), TypeRole);
        setData(game_path, FullPathRole);
        setData(qulonglong(program_id), ProgramIdRole);

        if (!Loader::IsValidSMDH(smdh_data)) {
            // SMDH is not valid, set a default icon
            setData(GetDefaultIcon(true), Qt::DecorationRole);
            return;
        }

        Loader::SMDH smdh;
        memcpy(&smdh, smdh_data.data(), sizeof(Loader::SMDH));

        // Get icon from SMDH
        setData(GetQPixmapFromSMDH(smdh, true), Qt::DecorationRole);

        // Get title from SMDH
        setData(GetQStringShortTitleFromSMDH(smdh, Loader::SMDH::TitleLanguage::English),
                TitleRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    QVariant data(int role) const override {
        if (role == Qt::DisplayRole) {
            std::string path, filename, extension;
            Common::SplitPath(data(FullPathRole).toString().toStdString(), &path, &filename,
                              &extension);
            QString title = data(TitleRole).toString();
            QString second_name = QString::fromStdString(filename + extension);
            static QRegExp installed_pattern(
                QString::fromStdString(
                    FileUtil::GetUserPath(D_SDMC_IDX) +
                    "Nintendo "
                    "3DS/00000000000000000000000000000000/00000000000000000000000000000000/"
                    "title/0004000(0|e)/[0-9a-f]{8}/content/")
                    .replace("\\", "\\\\"));
            static QRegExp system_pattern(
                QString::fromStdString(FileUtil::GetUserPath(D_NAND_IDX) +
                                       "00000000000000000000000000000000/"
                                       "title/00040010/[0-9a-f]{8}/content/")
                    .replace("\\", "\\\\"));
            if (installed_pattern.exactMatch(QString::fromStdString(path)) ||
                system_pattern.exactMatch(QString::fromStdString(path))) {
                // Use a different mechanism for system / installed titles showing program ID
                second_name = QString("%1-%2")
                                  .arg(data(ProgramIdRole).toULongLong(), 16, 16, QChar('0'))
                                  .toUpper()
                                  .arg(QString::fromStdString(filename));
            }
            return title + (title.isEmpty() ? "" : "\n     ") + second_name;
        } else {
            return GameListItem::data(role);
        }
    }
};

class GameListItemCompat : public GameListItem {
public:
    static const int CompatNumberRole = SortRole;

    GameListItemCompat() = default;
    explicit GameListItemCompat(const QString compatiblity) {
        setData(type(), TypeRole);

        auto iterator = status_data.find(compatiblity);
        if (iterator == status_data.end()) {
            LOG_WARNING(Frontend, "Invalid compatibility number {}", compatiblity.toStdString());
            return;
        }
        CompatStatus status = iterator->second;
        setData(compatiblity, CompatNumberRole);
        setText(QCoreApplication::translate("GameList", status.text));
        setToolTip(QCoreApplication::translate("GameList", status.tooltip));
        setData(CreateCirclePixmapFromColor(status.color), Qt::DecorationRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    bool operator<(const QStandardItem& other) const override {
        return data(CompatNumberRole) < other.data(CompatNumberRole);
    }
};

class GameListItemRegion : public GameListItem {
public:
    GameListItemRegion() = default;
    explicit GameListItemRegion(const std::vector<u8>& smdh_data) {
        setData(type(), TypeRole);

        if (!Loader::IsValidSMDH(smdh_data)) {
            setText(QObject::tr("Invalid region"));
            return;
        }

        Loader::SMDH smdh;
        memcpy(&smdh, smdh_data.data(), sizeof(Loader::SMDH));

        setText(GetRegionFromSMDH(smdh));
        setData(GetRegionFromSMDH(smdh), SortRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }
};

/**
 * A specialization of GameListItem for size values.
 * This class ensures that for every numerical size value it holds (in bytes), a correct
 * human-readable string representation will be displayed to the user.
 */
class GameListItemSize : public GameListItem {
public:
    static const int SizeRole = SortRole;

    GameListItemSize() = default;
    explicit GameListItemSize(const qulonglong size_bytes) {
        setData(type(), TypeRole);
        setData(size_bytes, SizeRole);
    }

    void setData(const QVariant& value, int role) override {
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

    int type() const override {
        return static_cast<int>(GameListItemType::Game);
    }

    /**
     * This operator is, in practice, only used by the TreeView sorting systems.
     * Override it so that it will correctly sort by numerical value instead of by string
     * representation.
     */
    bool operator<(const QStandardItem& other) const override {
        return data(SizeRole).toULongLong() < other.data(SizeRole).toULongLong();
    }
};

class GameListDir : public GameListItem {
public:
    static const int GameDirRole = Qt::UserRole + 2;

    explicit GameListDir(UISettings::GameDir& directory,
                         GameListItemType dir_type = GameListItemType::CustomDir)
        : dir_type{dir_type} {
        setData(type(), TypeRole);

        UISettings::GameDir* game_dir = &directory;
        setData(QVariant::fromValue(game_dir), GameDirRole);
        switch (dir_type) {
        case GameListItemType::InstalledDir:
            setData(QIcon::fromTheme("sd_card").pixmap(48), Qt::DecorationRole);
            setData("Installed Titles", Qt::DisplayRole);
            break;
        case GameListItemType::SystemDir:
            setData(QIcon::fromTheme("chip").pixmap(48), Qt::DecorationRole);
            setData("System Titles", Qt::DisplayRole);
            break;
        case GameListItemType::CustomDir:
            QString icon_name = QFileInfo::exists(game_dir->path) ? "folder" : "bad_folder";
            setData(QIcon::fromTheme(icon_name).pixmap(48), Qt::DecorationRole);
            setData(game_dir->path, Qt::DisplayRole);
            break;
        };
    };

    int type() const override {
        return static_cast<int>(dir_type);
    }

private:
    GameListItemType dir_type;
};

class GameListAddDir : public GameListItem {
public:
    explicit GameListAddDir() {
        setData(type(), TypeRole);
        setData(QIcon::fromTheme("plus").pixmap(48), Qt::DecorationRole);
        setData("Add New Game Directory", Qt::DisplayRole);
    }

    int type() const override {
        return static_cast<int>(GameListItemType::AddDir);
    }
};

/**
 * Asynchronous worker object for populating the game list.
 * Communicates with other threads through Qt's signal/slot system.
 */
class GameListWorker : public QObject, public QRunnable {
    Q_OBJECT

public:
    explicit GameListWorker(
        QList<UISettings::GameDir>& game_dirs,
        const std::unordered_map<std::string, std::pair<QString, QString>>& compatibility_list)
        : game_dirs(game_dirs), compatibility_list(compatibility_list) {}

public slots:
    /// Starts the processing of directory tree information.
    void run() override;
    /// Tells the worker that it should no longer continue processing. Thread-safe.
    void Cancel();

signals:
    /**
     * The `EntryReady` signal is emitted once an entry has been prepared and is ready
     * to be added to the game list.
     * @param entry_items a list with `QStandardItem`s that make up the columns of the new
     * entry.
     */
    void DirEntryReady(GameListDir* entry_items);
    void EntryReady(QList<QStandardItem*> entry_items, GameListDir* parent_dir);

    /**
     * After the worker has traversed the game directory looking for entries, this signal is
     * emitted with a list of folders that should be watched for changes as well.
     */
    void Finished(QStringList watch_list);

private:
    QStringList watch_list;
    const std::unordered_map<std::string, std::pair<QString, QString>>& compatibility_list;
    QList<UISettings::GameDir>& game_dirs;
    std::atomic_bool stop_processing;

    void AddFstEntriesToGameList(const std::string& dir_path, unsigned int recursion,
                                 GameListDir* parent_dir);
};

class GameList;
class QHBoxLayout;
class QTreeView;
class QLabel;
class QLineEdit;
class QToolButton;

class GameListSearchField : public QWidget {
    Q_OBJECT

public:
    explicit GameListSearchField(GameList* parent = nullptr);

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
        // EventFilter in order to process systemkeys while editing the searchfield
        bool eventFilter(QObject* obj, QEvent* event) override;
    };
    QHBoxLayout* layout_filter = nullptr;
    QTreeView* tree_view = nullptr;
    QLabel* label_filter = nullptr;
    QLineEdit* edit_filter = nullptr;
    QLabel* label_filter_result = nullptr;
    QToolButton* button_filter_close = nullptr;
};
