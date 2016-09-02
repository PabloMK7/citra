// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <clocale>
#include <memory>
#include <thread>

#include <glad/glad.h>

#define QT_NO_OPENGL
#include <QDesktopWidget>
#include <QtGui>
#include <QFileDialog>
#include <QMessageBox>
#include "qhexedit.h"

#include "citra_qt/bootmanager.h"
#include "citra_qt/config.h"
#include "citra_qt/configure_dialog.h"
#include "citra_qt/game_list.h"
#include "citra_qt/hotkeys.h"
#include "citra_qt/main.h"
#include "citra_qt/ui_settings.h"

// Debugger
#include "citra_qt/debugger/callstack.h"
#include "citra_qt/debugger/disassembler.h"
#include "citra_qt/debugger/graphics.h"
#include "citra_qt/debugger/graphics_breakpoints.h"
#include "citra_qt/debugger/graphics_cmdlists.h"
#include "citra_qt/debugger/graphics_surface.h"
#include "citra_qt/debugger/graphics_tracing.h"
#include "citra_qt/debugger/graphics_vertex_shader.h"
#include "citra_qt/debugger/profiler.h"
#include "citra_qt/debugger/ramview.h"
#include "citra_qt/debugger/registers.h"

#include "common/microprofile.h"
#include "common/platform.h"
#include "common/scm_rev.h"
#include "common/scope_exit.h"
#include "common/string_util.h"
#include "common/logging/backend.h"
#include "common/logging/filter.h"
#include "common/logging/log.h"
#include "common/logging/text_formatter.h"

#include "core/core.h"
#include "core/settings.h"
#include "core/system.h"
#include "core/arm/disassembler/load_symbol_map.h"
#include "core/gdbstub/gdbstub.h"
#include "core/loader/loader.h"

#include "video_core/video_core.h"

GMainWindow::GMainWindow() : config(new Config()), emu_thread(nullptr)
{
    Pica::g_debug_context = Pica::DebugContext::Construct();

    ui.setupUi(this);
    statusBar()->hide();

    render_window = new GRenderWindow(this, emu_thread.get());
    render_window->hide();

    game_list = new GameList();
    ui.horizontalLayout->addWidget(game_list);

    profilerWidget = new ProfilerWidget(this);
    addDockWidget(Qt::BottomDockWidgetArea, profilerWidget);
    profilerWidget->hide();

#if MICROPROFILE_ENABLED
    microProfileDialog = new MicroProfileDialog(this);
    microProfileDialog->hide();
#endif

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

    auto graphicsVertexShaderWidget = new GraphicsVertexShaderWidget(Pica::g_debug_context, this);
    addDockWidget(Qt::RightDockWidgetArea, graphicsVertexShaderWidget);
    graphicsVertexShaderWidget->hide();

    auto graphicsTracingWidget = new GraphicsTracingWidget(Pica::g_debug_context, this);
    addDockWidget(Qt::RightDockWidgetArea, graphicsTracingWidget);
    graphicsTracingWidget->hide();

    auto graphicsSurfaceViewerAction = new QAction(tr("Create Pica surface viewer"), this);
    connect(graphicsSurfaceViewerAction, SIGNAL(triggered()), this, SLOT(OnCreateGraphicsSurfaceViewer()));

    QMenu* debug_menu = ui.menu_View->addMenu(tr("Debugging"));
    debug_menu->addAction(graphicsSurfaceViewerAction);
    debug_menu->addSeparator();
    debug_menu->addAction(profilerWidget->toggleViewAction());
#if MICROPROFILE_ENABLED
    debug_menu->addAction(microProfileDialog->toggleViewAction());
#endif
    debug_menu->addAction(disasmWidget->toggleViewAction());
    debug_menu->addAction(registersWidget->toggleViewAction());
    debug_menu->addAction(callstackWidget->toggleViewAction());
    debug_menu->addAction(graphicsWidget->toggleViewAction());
    debug_menu->addAction(graphicsCommandsWidget->toggleViewAction());
    debug_menu->addAction(graphicsBreakpointsWidget->toggleViewAction());
    debug_menu->addAction(graphicsVertexShaderWidget->toggleViewAction());
    debug_menu->addAction(graphicsTracingWidget->toggleViewAction());

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
    restoreGeometry(UISettings::values.geometry);
    restoreState(UISettings::values.state);
    render_window->restoreGeometry(UISettings::values.renderwindow_geometry);
#if MICROPROFILE_ENABLED
    microProfileDialog->restoreGeometry(UISettings::values.microprofile_geometry);
    microProfileDialog->setVisible(UISettings::values.microprofile_visible);
#endif

    game_list->LoadInterfaceLayout();

    ui.action_Single_Window_Mode->setChecked(UISettings::values.single_window_mode);
    ToggleWindowMode();

    ui.actionDisplay_widget_title_bars->setChecked(UISettings::values.display_titlebar);
    OnDisplayTitleBars(ui.actionDisplay_widget_title_bars->isChecked());

    // Prepare actions for recent files
    for (int i = 0; i < max_recent_files_item; ++i) {
        actions_recent_files[i] = new QAction(this);
        actions_recent_files[i]->setVisible(false);
        connect(actions_recent_files[i], SIGNAL(triggered()), this, SLOT(OnMenuRecentFile()));

        ui.menu_recent_files->addAction(actions_recent_files[i]);
    }
    UpdateRecentFiles();

    // Setup connections
    connect(game_list, SIGNAL(GameChosen(QString)), this, SLOT(OnGameListLoadFile(QString)), Qt::DirectConnection);
    connect(ui.action_Configure, SIGNAL(triggered()), this, SLOT(OnConfigure()));
    connect(ui.action_Load_File, SIGNAL(triggered()), this, SLOT(OnMenuLoadFile()),Qt::DirectConnection);
    connect(ui.action_Load_Symbol_Map, SIGNAL(triggered()), this, SLOT(OnMenuLoadSymbolMap()));
    connect(ui.action_Select_Game_List_Root, SIGNAL(triggered()), this, SLOT(OnMenuSelectGameListRoot()));
    connect(ui.action_Start, SIGNAL(triggered()), this, SLOT(OnStartGame()));
    connect(ui.action_Pause, SIGNAL(triggered()), this, SLOT(OnPauseGame()));
    connect(ui.action_Stop, SIGNAL(triggered()), this, SLOT(OnStopGame()));
    connect(ui.action_Single_Window_Mode, SIGNAL(triggered(bool)), this, SLOT(ToggleWindowMode()));

    connect(this, SIGNAL(EmulationStarting(EmuThread*)), disasmWidget, SLOT(OnEmulationStarting(EmuThread*)));
    connect(this, SIGNAL(EmulationStopping()), disasmWidget, SLOT(OnEmulationStopping()));
    connect(this, SIGNAL(EmulationStarting(EmuThread*)), registersWidget, SLOT(OnEmulationStarting(EmuThread*)));
    connect(this, SIGNAL(EmulationStopping()), registersWidget, SLOT(OnEmulationStopping()));
    connect(this, SIGNAL(EmulationStarting(EmuThread*)), render_window, SLOT(OnEmulationStarting(EmuThread*)));
    connect(this, SIGNAL(EmulationStopping()), render_window, SLOT(OnEmulationStopping()));
    connect(this, SIGNAL(EmulationStarting(EmuThread*)), graphicsTracingWidget, SLOT(OnEmulationStarting(EmuThread*)));
    connect(this, SIGNAL(EmulationStopping()), graphicsTracingWidget, SLOT(OnEmulationStopping()));


    // Setup hotkeys
    RegisterHotkey("Main Window", "Load File", QKeySequence::Open);
    RegisterHotkey("Main Window", "Start Emulation");
    LoadHotkeys();

    connect(GetHotkey("Main Window", "Load File", this), SIGNAL(activated()), this, SLOT(OnMenuLoadFile()));
    connect(GetHotkey("Main Window", "Start Emulation", this), SIGNAL(activated()), this, SLOT(OnStartGame()));

    std::string window_title = Common::StringFromFormat("Citra | %s-%s", Common::g_scm_branch, Common::g_scm_desc);
    setWindowTitle(window_title.c_str());

    show();

    game_list->PopulateAsync(UISettings::values.gamedir, UISettings::values.gamedir_deepscan);

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

bool GMainWindow::InitializeSystem() {
    // Shutdown previous session if the emu thread is still active...
    if (emu_thread != nullptr)
        ShutdownGame();

    render_window->InitRenderTarget();
    render_window->MakeCurrent();

    if (!gladLoadGL()) {
        QMessageBox::critical(this, tr("Error while starting Citra!"),
                              tr("Failed to initialize the video core!\n\n"
                                 "Please ensure that your GPU supports OpenGL 3.3 and that you have the latest graphics driver."));
        return false;
    }

    // Initialize the core emulation
    System::Result system_result = System::Init(render_window);
    if (System::Result::Success != system_result) {
        switch (system_result) {
        case System::Result::ErrorInitVideoCore:
            QMessageBox::critical(this, tr("Error while starting Citra!"),
                                  tr("Failed to initialize the video core!\n\n"
                                     "Please ensure that your GPU supports OpenGL 3.3 and that you have the latest graphics driver."));
            break;

        default:
            QMessageBox::critical(this, tr("Error while starting Citra!"),
                                  tr("Unknown error (please check the log)!"));
            break;
        }
        return false;
    }
    return true;
}

bool GMainWindow::LoadROM(const std::string& filename) {
    std::unique_ptr<Loader::AppLoader> app_loader = Loader::GetLoader(filename);
    if (!app_loader) {
        LOG_CRITICAL(Frontend, "Failed to obtain loader for %s!", filename.c_str());
        QMessageBox::critical(this, tr("Error while loading ROM!"),
                              tr("The ROM format is not supported."));
        return false;
    }

    Loader::ResultStatus result = app_loader->Load();
    if (Loader::ResultStatus::Success != result) {
        LOG_CRITICAL(Frontend, "Failed to load ROM!");
        System::Shutdown();

        switch (result) {
        case Loader::ResultStatus::ErrorEncrypted: {
            // Build the MessageBox ourselves to have clickable link
            QMessageBox popup_error;
            popup_error.setTextFormat(Qt::RichText);
            popup_error.setWindowTitle(tr("Error while loading ROM!"));
            popup_error.setText(tr("The game that you are trying to load must be decrypted before being used with Citra.<br/><br/>"
                                  "For more information on dumping and decrypting games, please see: <a href='https://citra-emu.org/wiki/Dumping-Game-Cartridges'>https://citra-emu.org/wiki/Dumping-Game-Cartridges</a>"));
            popup_error.setIcon(QMessageBox::Critical);
            popup_error.exec();
            break;
        }
        case Loader::ResultStatus::ErrorInvalidFormat:
            QMessageBox::critical(this, tr("Error while loading ROM!"),
                                  tr("The ROM format is not supported."));
            break;
        case Loader::ResultStatus::Error:

        default:
            QMessageBox::critical(this, tr("Error while loading ROM!"),
                                  tr("Unknown error!"));
            break;
        }
        return false;
    }
    return true;
}

void GMainWindow::BootGame(const std::string& filename) {
    LOG_INFO(Frontend, "Citra starting...");
    StoreRecentFile(filename); // Put the filename on top of the list

    if (!InitializeSystem())
        return;

    if (!LoadROM(filename))
        return;

    // Create and start the emulation thread
    emu_thread = std::make_unique<EmuThread>(render_window);
    emit EmulationStarting(emu_thread.get());
    render_window->moveContext();
    emu_thread->start();

    connect(render_window, SIGNAL(Closed()), this, SLOT(OnStopGame()));
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
    if (ui.action_Single_Window_Mode->isChecked()) {
        game_list->hide();
    }
    render_window->show();

    emulation_running = true;
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

    // The emulation is stopped, so closing the window or not does not matter anymore
    disconnect(render_window, SIGNAL(Closed()), this, SLOT(OnStopGame()));

    // Update the GUI
    ui.action_Start->setEnabled(false);
    ui.action_Start->setText(tr("Start"));
    ui.action_Pause->setEnabled(false);
    ui.action_Stop->setEnabled(false);
    render_window->hide();
    game_list->show();

    emulation_running = false;
}

void GMainWindow::StoreRecentFile(const std::string& filename) {
    UISettings::values.recent_files.prepend(QString::fromStdString(filename));
    UISettings::values.recent_files.removeDuplicates();
    while (UISettings::values.recent_files.size() > max_recent_files_item) {
        UISettings::values.recent_files.removeLast();
    }

    UpdateRecentFiles();
}

void GMainWindow::UpdateRecentFiles() {
    unsigned int num_recent_files = std::min(UISettings::values.recent_files.size(), static_cast<int>(max_recent_files_item));

    for (unsigned int i = 0; i < num_recent_files; i++) {
        QString text = QString("&%1. %2").arg(i + 1).arg(QFileInfo(UISettings::values.recent_files[i]).fileName());
        actions_recent_files[i]->setText(text);
        actions_recent_files[i]->setData(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setToolTip(UISettings::values.recent_files[i]);
        actions_recent_files[i]->setVisible(true);
    }

    for (int j = num_recent_files; j < max_recent_files_item; ++j) {
        actions_recent_files[j]->setVisible(false);
    }

    // Grey out the recent files menu if the list is empty
    if (num_recent_files == 0) {
        ui.menu_recent_files->setEnabled(false);
    } else {
        ui.menu_recent_files->setEnabled(true);
    }
}

void GMainWindow::OnGameListLoadFile(QString game_path) {
    BootGame(game_path.toStdString());
}

void GMainWindow::OnMenuLoadFile() {
    QString filename = QFileDialog::getOpenFileName(this, tr("Load File"), UISettings::values.roms_path, tr("3DS executable (*.3ds *.3dsx *.elf *.axf *.cci *.cxi)"));
    if (!filename.isEmpty()) {
        UISettings::values.roms_path = QFileInfo(filename).path();

        BootGame(filename.toStdString());
    }
}

void GMainWindow::OnMenuLoadSymbolMap() {
    QString filename = QFileDialog::getOpenFileName(this, tr("Load Symbol Map"), UISettings::values.symbols_path, tr("Symbol map (*)"));
    if (!filename.isEmpty()) {
        UISettings::values.symbols_path = QFileInfo(filename).path();

        LoadSymbolMap(filename.toStdString());
    }
}

void GMainWindow::OnMenuSelectGameListRoot() {
    QString dir_path = QFileDialog::getExistingDirectory(this, tr("Select Directory"));
    if (!dir_path.isEmpty()) {
        UISettings::values.gamedir = dir_path;
        game_list->PopulateAsync(dir_path, UISettings::values.gamedir_deepscan);
    }
}

void GMainWindow::OnMenuRecentFile() {
    QAction* action = qobject_cast<QAction*>(sender());
    assert(action);

    QString filename = action->data().toString();
    QFileInfo file_info(filename);
    if (file_info.exists()) {
        BootGame(filename.toStdString());
    } else {
        // Display an error message and remove the file from the list.
        QMessageBox::information(this, tr("File not found"), tr("File \"%1\" not found").arg(filename));

        UISettings::values.recent_files.removeOne(filename);
        UpdateRecentFiles();
    }
}

void GMainWindow::OnStartGame() {
    emu_thread->SetRunning(true);

    ui.action_Start->setEnabled(false);
    ui.action_Start->setText(tr("Continue"));

    ui.action_Pause->setEnabled(true);
    ui.action_Stop->setEnabled(true);
}

void GMainWindow::OnPauseGame() {
    emu_thread->SetRunning(false);

    ui.action_Start->setEnabled(true);
    ui.action_Pause->setEnabled(false);
    ui.action_Stop->setEnabled(true);
}

void GMainWindow::OnStopGame() {
    ShutdownGame();
}

void GMainWindow::ToggleWindowMode() {
    if (ui.action_Single_Window_Mode->isChecked()) {
        // Render in the main window...
        render_window->BackupGeometry();
        ui.horizontalLayout->addWidget(render_window);
        render_window->setFocusPolicy(Qt::ClickFocus);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->setFocus();
            game_list->hide();
        }

    } else {
        // Render in a separate window...
        ui.horizontalLayout->removeWidget(render_window);
        render_window->setParent(nullptr);
        render_window->setFocusPolicy(Qt::NoFocus);
        if (emulation_running) {
            render_window->setVisible(true);
            render_window->RestoreGeometry();
            game_list->show();
        }
    }
}

void GMainWindow::OnConfigure() {
    ConfigureDialog configureDialog(this);
    auto result = configureDialog.exec();
    if (result == QDialog::Accepted)
    {
        configureDialog.applyConfiguration();
        render_window->ReloadSetKeymaps();
        config->Save();
    }
}

void GMainWindow::OnCreateGraphicsSurfaceViewer() {
    auto graphicsSurfaceViewerWidget = new GraphicsSurfaceWidget(Pica::g_debug_context, this);
    addDockWidget(Qt::RightDockWidgetArea, graphicsSurfaceViewerWidget);
    // TODO: Maybe graphicsSurfaceViewerWidget->setFloating(true);
    graphicsSurfaceViewerWidget->show();
}

bool GMainWindow::ConfirmClose() {
    if (emu_thread == nullptr || !UISettings::values.confirm_before_closing)
        return true;

    auto answer = QMessageBox::question(this, tr("Citra"),
                                        tr("Are you sure you want to close Citra?"),
                                        QMessageBox::Yes | QMessageBox::No, QMessageBox::No);
    return answer != QMessageBox::No;
}

void GMainWindow::closeEvent(QCloseEvent* event) {
    if (!ConfirmClose()) {
        event->ignore();
        return;
    }

    UISettings::values.geometry = saveGeometry();
    UISettings::values.state = saveState();
    UISettings::values.renderwindow_geometry = render_window->saveGeometry();
#if MICROPROFILE_ENABLED
    UISettings::values.microprofile_geometry = microProfileDialog->saveGeometry();
    UISettings::values.microprofile_visible = microProfileDialog->isVisible();
#endif
    UISettings::values.single_window_mode = ui.action_Single_Window_Mode->isChecked();
    UISettings::values.display_titlebar = ui.actionDisplay_widget_title_bars->isChecked();
    UISettings::values.first_start = false;

    game_list->SaveInterfaceLayout();
    SaveHotkeys();

    // Shutdown session if the emu thread is active...
    if (emu_thread != nullptr)
        ShutdownGame();

    render_window->close();

    QWidget::closeEvent(event);
}

#ifdef main
#undef main
#endif

int main(int argc, char* argv[]) {
    Log::Filter log_filter(Log::Level::Info);
    Log::SetFilter(&log_filter);

    MicroProfileOnThreadCreate("Frontend");
    SCOPE_EXIT({
        MicroProfileShutdown();
    });

    // Init settings params
    QCoreApplication::setOrganizationName("Citra team");
    QCoreApplication::setApplicationName("Citra");

    QApplication::setAttribute(Qt::AA_X11InitThreads);
    QApplication app(argc, argv);

    // Qt changes the locale and causes issues in float conversion using std::to_string() when generating shaders
    setlocale(LC_ALL, "C");

    GMainWindow main_window;
    // After settings have been loaded by GMainWindow, apply the filter
    log_filter.ParseFilterString(Settings::values.log_filter);

    main_window.show();
    return app.exec();
}
