#ifndef _CITRA_QT_MAIN_HXX_
#define _CITRA_QT_MAIN_HXX_

#include <QMainWindow>

#include "ui_main.h"

class GImageInfo;
class GRenderWindow;
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

private:
    void BootGame(const char* filename);

    void closeEvent(QCloseEvent* event);

private slots:
    void OnStartGame();
    void OnPauseGame();
    void OnStopGame();
    void OnMenuLoadFile();
    void OnMenuLoadSymbolMap();
    void OnOpenHotkeysDialog();
    void OnConfigure();
    void ToggleWindowMode();

private:
    Ui::MainWindow ui;

    GRenderWindow* render_window;

    DisassemblerWidget* disasmWidget;
    RegistersWidget* registersWidget;
    CallstackWidget* callstackWidget;
    GPUCommandStreamWidget* graphicsWidget;
    GPUCommandListWidget* graphicsCommandsWidget;
};

#endif // _CITRA_QT_MAIN_HXX_
