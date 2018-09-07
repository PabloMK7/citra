// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <QLabel>
#include <QMainWindow>
#include <QTimer>
#include <QTranslator>
#include "citra_qt/hotkeys.h"
#include "common/announce_multiplayer_room.h"
#include "core/core.h"
#include "core/hle/service/am/am.h"
#include "ui_main.h"

class AboutDialog;
class Config;
class ClickableLabel;
class EmuThread;
class GameList;
enum class GameListOpenTarget;
class GameListPlaceholder;
class GImageInfo;
class GPUCommandListWidget;
class GPUCommandStreamWidget;
class GraphicsBreakPointsWidget;
class GraphicsTracingWidget;
class GraphicsVertexShaderWidget;
class GRenderWindow;
class LLEServiceModulesWidget;
class MicroProfileDialog;
class MultiplayerState;
class ProfilerWidget;
template <typename>
class QFutureWatcher;
class QProgressBar;
class RegistersWidget;
class Updater;
class WaitTreeWidget;
namespace DiscordRPC {
class DiscordInterface;
}

class GMainWindow : public QMainWindow {
    Q_OBJECT

    /// Max number of recently loaded items to keep track of
    static const int max_recent_files_item = 10;

    // TODO: Make use of this!
    enum {
        UI_IDLE,
        UI_EMU_BOOTING,
        UI_EMU_RUNNING,
        UI_EMU_STOPPING,
    };

public:
    void filterBarSetChecked(bool state);
    void UpdateUITheme();

    GMainWindow();
    ~GMainWindow();

    GameList* game_list;
    std::unique_ptr<DiscordRPC::DiscordInterface> discord_rpc;

signals:

    /**
     * Signal that is emitted when a new EmuThread has been created and an emulation session is
     * about to start. At this time, the core system emulation has been initialized, and all
     * emulation handles and memory should be valid.
     *
     * @param emu_thread Pointer to the newly created EmuThread (to be used by widgets that need to
     *      access/change emulation state).
     */
    void EmulationStarting(EmuThread* emu_thread);

    /**
     * Signal that is emitted when emulation is about to stop. At this time, the EmuThread and core
     * system emulation handles and memory are still valid, but are about become invalid.
     */
    void EmulationStopping();

    void UpdateProgress(std::size_t written, std::size_t total);
    void CIAInstallReport(Service::AM::InstallStatus status, QString filepath);
    void CIAInstallFinished();
    // Signal that tells widgets to update icons to use the current theme
    void UpdateThemedIcons();

private:
    void InitializeWidgets();
    void InitializeDebugWidgets();
    void InitializeRecentFileMenuActions();
    void InitializeHotkeys();

    void SetDefaultUIGeometry();
    void SyncMenuUISettings();
    void RestoreUIState();

    void ConnectWidgetEvents();
    void ConnectMenuEvents();

    bool LoadROM(const QString& filename);
    void BootGame(const QString& filename);
    void ShutdownGame();

    void ShowTelemetryCallout();
    void ShowUpdaterWidgets();
    void ShowUpdatePrompt();
    void ShowNoUpdatePrompt();
    void CheckForUpdates();
    void SetDiscordEnabled(bool state);

    /**
     * Stores the filename in the recently loaded files list.
     * The new filename is stored at the beginning of the recently loaded files list.
     * After inserting the new entry, duplicates are removed meaning that if
     * this was inserted from \a OnMenuRecentFile(), the entry will be put on top
     * and remove from its previous position.
     *
     * Finally, this function calls \a UpdateRecentFiles() to update the UI.
     *
     * @param filename the filename to store
     */
    void StoreRecentFile(const QString& filename);

    /**
     * Updates the recent files menu.
     * Menu entries are rebuilt from the configuration file.
     * If there is no entry in the menu, the menu is greyed out.
     */
    void UpdateRecentFiles();

    /**
     * If the emulation is running,
     * asks the user if he really want to close the emulator
     *
     * @return true if the user confirmed
     */
    bool ConfirmClose();
    bool ConfirmChangeGame();
    void closeEvent(QCloseEvent* event) override;

private slots:
    void OnStartGame();
    void OnPauseGame();
    void OnStopGame();
    void OnMenuReportCompatibility();
    /// Called whenever a user selects a game in the game list widget.
    void OnGameListLoadFile(QString game_path);
    void OnGameListOpenFolder(u64 program_id, GameListOpenTarget target);
    void OnGameListNavigateToGamedbEntry(
        u64 program_id,
        std::unordered_map<std::string, std::pair<QString, QString>>& compatibility_list);
    void OnGameListOpenDirectory(QString path);
    void OnGameListAddDirectory();
    void OnGameListShowList(bool show);
    void OnMenuLoadFile();
    void OnMenuInstallCIA();
    void OnUpdateProgress(std::size_t written, std::size_t total);
    void OnCIAInstallReport(Service::AM::InstallStatus status, QString filepath);
    void OnCIAInstallFinished();
    void OnMenuRecentFile();
    void OnConfigure();
    void OnToggleFilterBar();
    void OnDisplayTitleBars(bool);
    void ToggleFullscreen();
    void ChangeScreenLayout();
    void ToggleScreenLayout();
    void OnSwapScreens();
    void ShowFullscreen();
    void HideFullscreen();
    void ToggleWindowMode();
    void OnCreateGraphicsSurfaceViewer();
    void OnRecordMovie();
    void OnPlayMovie();
    void OnStopRecordingPlayback();
    void OnCoreError(Core::System::ResultStatus, std::string);
    /// Called whenever a user selects Help->About Citra
    void OnMenuAboutCitra();
    void OnUpdateFound(bool found, bool error);
    void OnCheckForUpdates();
    void OnOpenUpdater();
    void OnLanguageChanged(const QString& locale);

private:
    bool ValidateMovie(const QString& path, u64 program_id = 0);
    Q_INVOKABLE void OnMoviePlaybackCompleted();
    void UpdateStatusBar();
    void LoadTranslation();
    void SetupUIStrings();
    void RetranslateStatusBar();

    Ui::MainWindow ui;

    GRenderWindow* render_window;

    GameListPlaceholder* game_list_placeholder;

    // Status bar elements
    QProgressBar* progress_bar = nullptr;
    QLabel* message_label = nullptr;
    QLabel* emu_speed_label = nullptr;
    QLabel* game_fps_label = nullptr;
    QLabel* emu_frametime_label = nullptr;
    QTimer status_bar_update_timer;

    MultiplayerState* multiplayer_state = nullptr;
    std::unique_ptr<Config> config;

    // Whether emulation is currently running in Citra.
    bool emulation_running = false;
    std::unique_ptr<EmuThread> emu_thread;
    // The title of the game currently running
    QString game_title;
    // The path to the game currently running
    QString game_path;

    // Movie
    bool movie_record_on_start = false;
    QString movie_record_path;

    // Debugger panes
    ProfilerWidget* profilerWidget;
    MicroProfileDialog* microProfileDialog;
    RegistersWidget* registersWidget;
    GPUCommandStreamWidget* graphicsWidget;
    GPUCommandListWidget* graphicsCommandsWidget;
    GraphicsBreakPointsWidget* graphicsBreakpointsWidget;
    GraphicsVertexShaderWidget* graphicsVertexShaderWidget;
    GraphicsTracingWidget* graphicsTracingWidget;
    LLEServiceModulesWidget* lleServiceModulesWidget;
    WaitTreeWidget* waitTreeWidget;
    Updater* updater;

    bool explicit_update_check = false;
    bool defer_update_prompt = false;

    QAction* actions_recent_files[max_recent_files_item];

    QTranslator translator;

    // stores default icon theme search paths for the platform
    QStringList default_theme_paths;

    HotkeyRegistry hotkey_registry;

protected:
    void dropEvent(QDropEvent* event) override;
    void dragEnterEvent(QDragEnterEvent* event) override;
    void dragMoveEvent(QDragMoveEvent* event) override;
};

Q_DECLARE_METATYPE(std::size_t);
Q_DECLARE_METATYPE(Service::AM::InstallStatus);
