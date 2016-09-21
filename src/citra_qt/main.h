// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifndef _CITRA_QT_MAIN_HXX_
#define _CITRA_QT_MAIN_HXX_

#include <memory>
#include <QMainWindow>
#include "ui_main.h"

class Config;
class GameList;
class GImageInfo;
class GRenderWindow;
class EmuThread;
class ProfilerWidget;
class MicroProfileDialog;
class DisassemblerWidget;
class RegistersWidget;
class CallstackWidget;
class GPUCommandStreamWidget;
class GPUCommandListWidget;

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
    GMainWindow();
    ~GMainWindow();

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

private:
    bool InitializeSystem();
    bool LoadROM(const std::string& filename);
    void BootGame(const std::string& filename);
    void ShutdownGame();

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
    void StoreRecentFile(const std::string& filename);

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
    void closeEvent(QCloseEvent* event) override;

private slots:
    void OnStartGame();
    void OnPauseGame();
    void OnStopGame();
    /// Called whenever a user selects a game in the game list widget.
    void OnGameListLoadFile(QString game_path);
    void OnMenuLoadFile();
    void OnMenuLoadSymbolMap();
    /// Called whenever a user selects the "File->Select Game List Root" menu item
    void OnMenuSelectGameListRoot();
    void OnMenuRecentFile();
    void OnConfigure();
    void OnDisplayTitleBars(bool);
    void ToggleWindowMode();
    void OnCreateGraphicsSurfaceViewer();

private:
    Ui::MainWindow ui;

    GRenderWindow* render_window;
    GameList* game_list;

    std::unique_ptr<Config> config;

    // Whether emulation is currently running in Citra.
    bool emulation_running = false;
    std::unique_ptr<EmuThread> emu_thread;

    ProfilerWidget* profilerWidget;
    MicroProfileDialog* microProfileDialog;
    DisassemblerWidget* disasmWidget;
    RegistersWidget* registersWidget;
    CallstackWidget* callstackWidget;
    GPUCommandStreamWidget* graphicsWidget;
    GPUCommandListWidget* graphicsCommandsWidget;

    QAction* actions_recent_files[max_recent_files_item];
};

#endif // _CITRA_QT_MAIN_HXX_
