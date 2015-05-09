// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <thread>

#include <QtGui>
#include <QDesktopWidget>
#include <QFileDialog>
#include "qhexedit.h"
#include "main.h"

#include "common/logging/text_formatter.h"
#include "common/logging/log.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/make_unique.h"
#include "common/platform.h"
#include "common/scope_exit.h"

#if EMU_PLATFORM == PLATFORM_LINUX
#include <unistd.h>
#endif

#include "bootmanager.h"
#include "hotkeys.h"

//debugger
#include "debugger/disassembler.h"
#include "debugger/registers.h"
#include "debugger/callstack.h"
#include "debugger/ramview.h"
#include "debugger/graphics.h"
#include "debugger/graphics_breakpoints.h"
#include "debugger/graphics_cmdlists.h"
#include "debugger/graphics_framebuffer.h"
#include "debugger/graphics_vertex_shader.h"
#include "debugger/profiler.h"

#include "core/settings.h"
#include "core/system.h"
#include "core/core.h"
#include "core/loader/loader.h"
#include "core/arm/disassembler/load_symbol_map.h"
#include "citra_qt/config.h"

#include "version.h"

GMainWindow::GMainWindow() : emu_thread(nullptr)
{
    Pica::g_debug_context = Pica::DebugContext::Construct();

    Config config;

    ui.setupUi(this);
    statusBar()->hide();

    render_window = new GRenderWindow(this, emu_thread.get());
    render_window->hide();

    profilerWidget = new ProfilerWidget(this);
    addDockWidget(Qt::BottomDockWidgetArea, profilerWidget);
    profilerWidget->hide();

    disasmWidget = new DisassemblerWidget(this, emu_thread.get());
    addDockWidget(Qt::BottomDockWidgetArea, disasmWidget);
    disasmWidget->hide();

    registersWidget = new RegistersWidget(this);
    addDockWidget(Qt::RightDockWidgetArea, registersWidget);
    registersWidget->hide();

    callstackWidget = new CallstackWidget(this);
    addDockWidget(Qt::RightDockWidgetArea, callstackWidget);
    callstackWidget->hide();

    graphicsWidget = new GPUCommandStreamWidget(this);
    addDockWidget(Qt::RightDockWidgetArea, graphicsWidget);
    graphicsWidget ->hide();

    graphicsCommandsWidget = new GPUCommandListWidget(this);
    addDockWidget(Qt::RightDockWidgetArea, graphicsCommandsWidget);
    graphicsCommandsWidget->hide();

    auto graphicsBreakpointsWidget = new GraphicsBreakPointsWidget(Pica::g_debug_context, this);
    addDockWidget(Qt::RightDockWidgetArea, graphicsBreakpointsWidget);
    graphicsBreakpointsWidget->hide();

    auto graphicsFramebufferWidget = new GraphicsFramebufferWidget(Pica::g_debug_context, this);
    addDockWidget(Qt::RightDockWidgetArea, graphicsFramebufferWidget);
    graphicsFramebufferWidget->hide();

    auto graphicsVertexShaderWidget = new GraphicsVertexShaderWidget(Pica::g_debug_context, this);
    addDockWidget(Qt::RightDockWidgetArea, graphicsVertexShaderWidget);
    graphicsVertexShaderWidget->hide();

    QMenu* debug_menu = ui.menu_View->addMenu(tr("Debugging"));
    debug_menu->addAction(profilerWidget->toggleViewAction());
    debug_menu->addAction(disasmWidget->toggleViewAction());
    debug_menu->addAction(registersWidget->toggleViewAction());
    debug_menu->addAction(callstackWidget->toggleViewAction());
    debug_menu->addAction(graphicsWidget->toggleViewAction());
    debug_menu->addAction(graphicsCommandsWidget->toggleViewAction());
    debug_menu->addAction(graphicsBreakpointsWidget->toggleViewAction());
    debug_menu->addAction(graphicsFramebufferWidget->toggleViewAction());
    debug_menu->addAction(graphicsVertexShaderWidget->toggleViewAction());

    // Set default UI state
    // geometry: 55% of the window contents are in the upper screen half, 45% in the lower half
    QDesktopWidget* desktop = ((QApplication*)QApplication::instance())->desktop();
    QRect screenRect = desktop->screenGeometry(this);
    int x, y, w, h;
    w = screenRect.width() * 2 / 3;
    h = screenRect.height() / 2;
    x = (screenRect.x() + screenRect.width()) / 2 - w / 2;
    y = (screenRect.y() + screenRect.height()) / 2 - h * 55 / 100;
    setGeometry(x, y, w, h);


    // Restore UI state
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Citra team", "Citra");
    restoreGeometry(settings.value("geometry").toByteArray());
    restoreState(settings.value("state").toByteArray());
    render_window->restoreGeometry(settings.value("geometryRenderWindow").toByteArray());

    ui.action_Single_Window_Mode->setChecked(settings.value("singleWindowMode", true).toBool());
    ToggleWindowMode();

    ui.actionDisplay_widget_title_bars->setChecked(settings.value("displayTitleBars", true).toBool());
    OnDisplayTitleBars(ui.actionDisplay_widget_title_bars->isChecked());

    // Setup connections
    connect(ui.action_Load_File, SIGNAL(triggered()), this, SLOT(OnMenuLoadFile()));
    connect(ui.action_Load_Symbol_Map, SIGNAL(triggered()), this, SLOT(OnMenuLoadSymbolMap()));
    connect(ui.action_Start, SIGNAL(triggered()), this, SLOT(OnStartGame()));
    connect(ui.action_Pause, SIGNAL(triggered()), this, SLOT(OnPauseGame()));
    connect(ui.action_Stop, SIGNAL(triggered()), this, SLOT(OnStopGame()));
    connect(ui.action_Single_Window_Mode, SIGNAL(triggered(bool)), this, SLOT(ToggleWindowMode()));
    connect(ui.action_Hotkeys, SIGNAL(triggered()), this, SLOT(OnOpenHotkeysDialog()));

    connect(this, SIGNAL(EmulationStarting(EmuThread*)), disasmWidget, SLOT(OnEmulationStarting(EmuThread*)));
    connect(this, SIGNAL(EmulationStopping()), disasmWidget, SLOT(OnEmulationStopping()));
    connect(this, SIGNAL(EmulationStarting(EmuThread*)), registersWidget, SLOT(OnEmulationStarting(EmuThread*)));
    connect(this, SIGNAL(EmulationStopping()), registersWidget, SLOT(OnEmulationStopping()));
    connect(this, SIGNAL(EmulationStarting(EmuThread*)), render_window, SLOT(OnEmulationStarting(EmuThread*)));
    connect(this, SIGNAL(EmulationStopping()), render_window, SLOT(OnEmulationStopping()));

    // Setup hotkeys
    RegisterHotkey("Main Window", "Load File", QKeySequence::Open);
    RegisterHotkey("Main Window", "Start Emulation");
    LoadHotkeys(settings);

    connect(GetHotkey("Main Window", "Load File", this), SIGNAL(activated()), this, SLOT(OnMenuLoadFile()));
    connect(GetHotkey("Main Window", "Start Emulation", this), SIGNAL(activated()), this, SLOT(OnStartGame()));

    std::string window_title = Common::StringFromFormat("Citra | %s-%s", Common::g_scm_branch, Common::g_scm_desc);
    setWindowTitle(window_title.c_str());

    show();

    QStringList args = QApplication::arguments();
    if (args.length() >= 2) {
        BootGame(args[1].toStdString());
    }
}

GMainWindow::~GMainWindow()
{
    // will get automatically deleted otherwise
    if (render_window->parent() == nullptr)
        delete render_window;

    Pica::g_debug_context.reset();
}

void GMainWindow::OnDisplayTitleBars(bool show)
{
    QList<QDockWidget*> widgets = findChildren<QDockWidget*>();

    if (show) {
        for (QDockWidget* widget: widgets) {
            QWidget* old = widget->titleBarWidget();
            widget->setTitleBarWidget(nullptr);
            if (old != nullptr)
                delete old;
        }
    } else {
        for (QDockWidget* widget: widgets) {
            QWidget* old = widget->titleBarWidget();
            widget->setTitleBarWidget(new QWidget());
            if (old != nullptr)
                delete old;
        }
    }
}

void GMainWindow::BootGame(std::string filename) {
    LOG_INFO(Frontend, "Citra starting...\n");

    // Initialize the core emulation
    System::Init(render_window);

    // Load the game
    if (Loader::ResultStatus::Success != Loader::LoadFile(filename)) {
        LOG_CRITICAL(Frontend, "Failed to load ROM!");
        System::Shutdown();
        return;
    }

    // Create and start the emulation thread
    emu_thread = Common::make_unique<EmuThread>(render_window);
    emit EmulationStarting(emu_thread.get());
    emu_thread->start();

    // BlockingQueuedConnection is important here, it makes sure we've finished refreshing our views before the CPU continues
    connect(emu_thread.get(), SIGNAL(DebugModeEntered()), disasmWidget, SLOT(OnDebugModeEntered()), Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), SIGNAL(DebugModeEntered()), registersWidget, SLOT(OnDebugModeEntered()), Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), SIGNAL(DebugModeEntered()), callstackWidget, SLOT(OnDebugModeEntered()), Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), SIGNAL(DebugModeLeft()), disasmWidget, SLOT(OnDebugModeLeft()), Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), SIGNAL(DebugModeLeft()), registersWidget, SLOT(OnDebugModeLeft()), Qt::BlockingQueuedConnection);
    connect(emu_thread.get(), SIGNAL(DebugModeLeft()), callstackWidget, SLOT(OnDebugModeLeft()), Qt::BlockingQueuedConnection);

    // Update the GUI
    registersWidget->OnDebugModeEntered();
    callstackWidget->OnDebugModeEntered();
    render_window->show();

    OnStartGame();
}

void GMainWindow::ShutdownGame() {
    emu_thread->RequestStop();

    // Release emu threads from any breakpoints
    // This belongs after RequestStop() and before wait() because if emulation stops on a GPU
    // breakpoint after (or before) RequestStop() is called, the emulation would never be able
    // to continue out to the main loop and terminate. Thus wait() would hang forever.
    // TODO(bunnei): This function is not thread safe, but it's being used as if it were
    Pica::g_debug_context->ClearBreakpoints();

    emit EmulationStopping();

    // Wait for emulation thread to complete and delete it
    emu_thread->wait();
    emu_thread = nullptr;

    // Shutdown the core emulation
    System::Shutdown();

    // Update the GUI
    ui.action_Start->setEnabled(false);
    ui.action_Pause->setEnabled(false);
    ui.action_Stop->setEnabled(false);
    render_window->hide();
}

void GMainWindow::OnMenuLoadFile()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Load File"), QString(), tr("3DS executable (*.3ds *.3dsx *.elf *.axf *.cci *.cxi)"));
    if (filename.size()) {
        // Shutdown previous session if the emu thread is still active...
        if (emu_thread != nullptr)
            ShutdownGame();

        BootGame(filename.toLatin1().data());
    }
}

void GMainWindow::OnMenuLoadSymbolMap() {
    QString filename = QFileDialog::getOpenFileName(this, tr("Load Symbol Map"), QString(), tr("Symbol map (*)"));
    if (filename.size())
        LoadSymbolMap(filename.toLatin1().data());
}

void GMainWindow::OnStartGame()
{
    emu_thread->SetRunning(true);

    ui.action_Start->setEnabled(false);
    ui.action_Pause->setEnabled(true);
    ui.action_Stop->setEnabled(true);
}

void GMainWindow::OnPauseGame()
{
    emu_thread->SetRunning(false);

    ui.action_Start->setEnabled(true);
    ui.action_Pause->setEnabled(false);
    ui.action_Stop->setEnabled(true);
}

void GMainWindow::OnStopGame() {
    ShutdownGame();
}

void GMainWindow::OnOpenHotkeysDialog()
{
    GHotkeysDialog dialog(this);
    dialog.exec();
}


void GMainWindow::ToggleWindowMode() {
    if (ui.action_Single_Window_Mode->isChecked()) {
        // Render in the main window...
        render_window->BackupGeometry();
        ui.horizontalLayout->addWidget(render_window);
        render_window->setVisible(true);
        render_window->setFocusPolicy(Qt::ClickFocus);
        render_window->setFocus();

    } else {
        // Render in a separate window...
        ui.horizontalLayout->removeWidget(render_window);
        render_window->setParent(nullptr);
        render_window->setVisible(true);
        render_window->RestoreGeometry();
        render_window->setFocusPolicy(Qt::NoFocus);
    }
}

void GMainWindow::OnConfigure()
{
    //GControllerConfigDialog* dialog = new GControllerConfigDialog(controller_ports, this);
}

void GMainWindow::closeEvent(QCloseEvent* event)
{
    // Save window layout
    QSettings settings(QSettings::IniFormat, QSettings::UserScope, "Citra team", "Citra");
    settings.setValue("geometry", saveGeometry());
    settings.setValue("state", saveState());
    settings.setValue("geometryRenderWindow", render_window->saveGeometry());
    settings.setValue("singleWindowMode", ui.action_Single_Window_Mode->isChecked());
    settings.setValue("displayTitleBars", ui.actionDisplay_widget_title_bars->isChecked());
    settings.setValue("firstStart", false);
    SaveHotkeys(settings);

    ShutdownGame();

    render_window->close();

    QWidget::closeEvent(event);
}

#ifdef main
#undef main
#endif

int main(int argc, char* argv[])
{
    std::shared_ptr<Log::Logger> logger = Log::InitGlobalLogger();
    Log::Filter log_filter(Log::Level::Info);
    Log::SetFilter(&log_filter);
    std::thread logging_thread(Log::TextLoggingLoop, logger);
    SCOPE_EXIT({
        logger->Close();
        logging_thread.join();
    });

    QApplication::setAttribute(Qt::AA_X11InitThreads);
    QApplication app(argc, argv);

    GMainWindow main_window;
    // After settings have been loaded by GMainWindow, apply the filter
    log_filter.ParseFilterString(Settings::values.log_filter);

    main_window.show();
    return app.exec();
}
