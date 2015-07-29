// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#ifndef _CITRA_QT_MAIN_HXX_
#define _CITRA_QT_MAIN_HXX_

#include <memory>
#include <QMainWindow>

#include "ui_main.h"

class GImageInfo;
class GRenderWindow;
class EmuThread;
class ProfilerWidget;
class DisassemblerWidget;
class RegistersWidget;
class CallstackWidget;
class GPUCommandStreamWidget;
class GPUCommandListWidget;

class GMainWindow : public QMainWindow
{
    Q_OBJECT

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
    void BootGame(const std::string& filename);
    void ShutdownGame();

    void closeEvent(QCloseEvent* event) override;

private slots:
    void OnStartGame();
    void OnPauseGame();
    void OnStopGame();
    void OnMenuLoadFile();
    void OnMenuLoadSymbolMap();
    void OnOpenHotkeysDialog();
    void OnConfigure();
    void OnDisplayTitleBars(bool);
    void SetHardwareRendererEnabled(bool);
    void ToggleWindowMode();

private:
    Ui::MainWindow ui;

    GRenderWindow* render_window;

    std::unique_ptr<EmuThread> emu_thread;

    ProfilerWidget* profilerWidget;
    DisassemblerWidget* disasmWidget;
    RegistersWidget* registersWidget;
    CallstackWidget* callstackWidget;
    GPUCommandStreamWidget* graphicsWidget;
    GPUCommandListWidget* graphicsCommandsWidget;
};

#endif // _CITRA_QT_MAIN_HXX_
