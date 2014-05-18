#include <QtGui>
#include <QDesktopWidget>
#include <QFileDialog>
#include "qhexedit.h"
#include "main.hxx"

#include "common/common.h"
#include "common/platform.h"
#include "common/log_manager.h"
#if EMU_PLATFORM == PLATFORM_LINUX
#include <unistd.h>
#endif

#include "bootmanager.hxx"
#include "hotkeys.hxx"

//debugger
#include "debugger/disassembler.hxx"
#include "debugger/registers.hxx"
#include "debugger/callstack.hxx"
#include "debugger/ramview.hxx"
#include "debugger/graphics.hxx"
#include "debugger/graphics_cmdlists.hxx"

#include "core/system.h"
#include "core/loader.h"
#include "core/core.h"
#include "core/arm/disassembler/load_symbol_map.h"
#include "version.h"


GMainWindow::GMainWindow()
{
    ui.setupUi(this);
    statusBar()->hide();

    render_window = new GRenderWindow;
    render_window->hide();

    disasmWidget = new DisassemblerWidget(this, render_window->GetEmuThread());
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
    callstackWidget->hide();

    graphicsCommandsWidget = new GPUCommandListWidget(this);
    addDockWidget(Qt::RightDockWidgetArea, graphicsCommandsWidget);
    callstackWidget->hide();

    QMenu* debug_menu = ui.menu_View->addMenu(tr("Debugging"));
    debug_menu->addAction(disasmWidget->toggleViewAction());
    debug_menu->addAction(registersWidget->toggleViewAction());
    debug_menu->addAction(callstackWidget->toggleViewAction());
    debug_menu->addAction(graphicsWidget->toggleViewAction());
    debug_menu->addAction(graphicsCommandsWidget->toggleViewAction());

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

    ui.action_Popout_Window_Mode->setChecked(settings.value("popoutWindowMode", true).toBool());
    ToggleWindowMode();

    // Setup connections
    connect(ui.action_Load_File, SIGNAL(triggered()), this, SLOT(OnMenuLoadFile()));
    connect(ui.action_Load_Symbol_Map, SIGNAL(triggered()), this, SLOT(OnMenuLoadSymbolMap()));
    connect(ui.action_Start, SIGNAL(triggered()), this, SLOT(OnStartGame()));
    connect(ui.action_Pause, SIGNAL(triggered()), this, SLOT(OnPauseGame()));
    connect(ui.action_Stop, SIGNAL(triggered()), this, SLOT(OnStopGame()));
    connect(ui.action_Popout_Window_Mode, SIGNAL(triggered(bool)), this, SLOT(ToggleWindowMode()));
    connect(ui.action_Hotkeys, SIGNAL(triggered()), this, SLOT(OnOpenHotkeysDialog()));

    // BlockingQueuedConnection is important here, it makes sure we've finished refreshing our views before the CPU continues
    connect(&render_window->GetEmuThread(), SIGNAL(CPUStepped()), disasmWidget, SLOT(OnCPUStepped()), Qt::BlockingQueuedConnection);
    connect(&render_window->GetEmuThread(), SIGNAL(CPUStepped()), registersWidget, SLOT(OnCPUStepped()), Qt::BlockingQueuedConnection);
    connect(&render_window->GetEmuThread(), SIGNAL(CPUStepped()), callstackWidget, SLOT(OnCPUStepped()), Qt::BlockingQueuedConnection);

    // Setup hotkeys
    RegisterHotkey("Main Window", "Load File", QKeySequence::Open);
    RegisterHotkey("Main Window", "Start Emulation");
    LoadHotkeys(settings);

    connect(GetHotkey("Main Window", "Load File", this), SIGNAL(activated()), this, SLOT(OnMenuLoadFile()));
    connect(GetHotkey("Main Window", "Start Emulation", this), SIGNAL(activated()), this, SLOT(OnStartGame()));

    setWindowTitle(render_window->GetWindowTitle().c_str());

    show();

    LogManager::Init();
    System::Init(render_window);
}

GMainWindow::~GMainWindow()
{
    // will get automatically deleted otherwise
    if (render_window->parent() == NULL)
        delete render_window;
}

void GMainWindow::BootGame(const char* filename)
{
    NOTICE_LOG(MASTER_LOG, "citra starting...\n");

    if (Core::Init()) {
        ERROR_LOG(MASTER_LOG, "core initialization failed, exiting...");
        Core::Stop();
        exit(1);
    }

    // Load a game or die...
    std::string boot_filename = filename;
    std::string error_str;
    bool res = Loader::LoadFile(boot_filename, &error_str);

    if (!res) {
        ERROR_LOG(BOOT, "Failed to load ROM: %s", error_str.c_str());
    }

    disasmWidget->Init();
    registersWidget->OnCPUStepped();
    callstackWidget->OnCPUStepped();

    render_window->DoneCurrent(); // make sure EmuThread can access GL context
    render_window->GetEmuThread().SetFilename(filename);
    render_window->GetEmuThread().start();

    render_window->show();
}

void GMainWindow::OnMenuLoadFile()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Load file"), QString(), tr("3DS homebrew (*.elf *.axf *.dat *.bin)"));
    if (filename.size())
       BootGame(filename.toLatin1().data());
}

void GMainWindow::OnMenuLoadSymbolMap() {
    QString filename = QFileDialog::getOpenFileName(this, tr("Load symbol map"), QString(), tr("Symbol map (*)"));
    if (filename.size())
        LoadSymbolMap(filename.toLatin1().data());
}

void GMainWindow::OnStartGame()
{
    render_window->GetEmuThread().SetCpuRunning(true);

    ui.action_Start->setEnabled(false);
    ui.action_Pause->setEnabled(true);
    ui.action_Stop->setEnabled(true);
}

void GMainWindow::OnPauseGame()
{
    render_window->GetEmuThread().SetCpuRunning(false);

    ui.action_Start->setEnabled(true);
    ui.action_Pause->setEnabled(false);
    ui.action_Stop->setEnabled(true);
}

void GMainWindow::OnStopGame()
{
    render_window->GetEmuThread().SetCpuRunning(false);

    ui.action_Start->setEnabled(true);
    ui.action_Pause->setEnabled(false);
    ui.action_Stop->setEnabled(false);
}

void GMainWindow::OnOpenHotkeysDialog()
{
    GHotkeysDialog dialog(this);
    dialog.exec();
}


void GMainWindow::ToggleWindowMode()
{
    bool enable = ui.action_Popout_Window_Mode->isChecked();
    if (enable && render_window->parent() != NULL)
    {
        ui.horizontalLayout->removeWidget(render_window);
        render_window->setParent(NULL);
        render_window->setVisible(true);
        render_window->DoneCurrent();
        render_window->RestoreGeometry();
    }
    else if (!enable && render_window->parent() == NULL)
    {
        render_window->BackupGeometry();
        ui.horizontalLayout->addWidget(render_window);
        render_window->setVisible(true);
        render_window->DoneCurrent();
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
    settings.setValue("popoutWindowMode", ui.action_Popout_Window_Mode->isChecked());
    settings.setValue("firstStart", false);
    SaveHotkeys(settings);

    render_window->close();

    QWidget::closeEvent(event);
}

#ifdef main
#undef main
#endif

int __cdecl main(int argc, char* argv[])
{
    QApplication::setAttribute(Qt::AA_X11InitThreads);
    QApplication app(argc, argv);
    GMainWindow main_window;

    main_window.show();
    return app.exec();
}
