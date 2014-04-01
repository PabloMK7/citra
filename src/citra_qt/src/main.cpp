#include <QtGui>
#include <QDesktopWidget>
#include <QFileDialog>
#include "qhexedit.h"
#include "main.hxx"

#include "common.h"
#include "platform.h"
#if EMU_PLATFORM == PLATFORM_LINUX
#include <unistd.h>
#endif

#include "bootmanager.hxx"
#include "hotkeys.hxx"

//debugger
#include "disasm.hxx"
#include "cpu_regs.hxx"
#include "callstack.hxx"
#include "ramview.hxx"

#include "core.h"
#include "version.h"


GMainWindow::GMainWindow()
{
    ui.setupUi(this);
    statusBar()->hide();

    render_window = new GRenderWindow;
    render_window->hide();

    GDisAsmView* disasm = new GDisAsmView(this, render_window->GetEmuThread());
    addDockWidget(Qt::BottomDockWidgetArea, disasm);
    disasm->hide();

    GARM11RegsView* arm_regs = new GARM11RegsView(this);
    addDockWidget(Qt::RightDockWidgetArea, arm_regs);
    arm_regs->hide();

    QMenu* debug_menu = ui.menu_View->addMenu(tr("Debugging"));
    debug_menu->addAction(disasm->toggleViewAction());
    debug_menu->addAction(arm_regs->toggleViewAction());

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

    ui.action_Single_Window_Mode->setChecked(settings.value("singleWindowMode", false).toBool());
    SetupEmuWindowMode();

    // Setup connections
    connect(ui.action_load_elf, SIGNAL(triggered()), this, SLOT(OnMenuLoadELF()));
	connect(ui.action_Start, SIGNAL(triggered()), this, SLOT(OnStartGame()));
	connect(ui.action_Pause, SIGNAL(triggered()), this, SLOT(OnPauseGame()));
	connect(ui.action_Stop, SIGNAL(triggered()), this, SLOT(OnStopGame()));
	connect(ui.action_Single_Window_Mode, SIGNAL(triggered(bool)), this, SLOT(SetupEmuWindowMode()));
    connect(ui.action_Hotkeys, SIGNAL(triggered()), this, SLOT(OnOpenHotkeysDialog()));

    // BlockingQueuedConnection is important here, it makes sure we've finished refreshing our views before the CPU continues
    connect(&render_window->GetEmuThread(), SIGNAL(CPUStepped()), disasm, SLOT(OnCPUStepped()), Qt::BlockingQueuedConnection);
    connect(&render_window->GetEmuThread(), SIGNAL(CPUStepped()), arm_regs, SLOT(OnCPUStepped()), Qt::BlockingQueuedConnection);
	//connect(&render_window->GetEmuThread(), SIGNAL(CPUStepped()), ram_edit, SLOT(OnCPUStepped()), Qt::BlockingQueuedConnection);
	//connect(&render_window->GetEmuThread(), SIGNAL(CPUStepped()), callstack, SLOT(OnCPUStepped()), Qt::BlockingQueuedConnection);

    // Setup hotkeys
    RegisterHotkey("Main Window", "Load Image", QKeySequence::Open);
    RegisterHotkey("Main Window", "Start Emulation");
    LoadHotkeys(settings);

    connect(GetHotkey("Main Window", "Load Image", this), SIGNAL(activated()), this, SLOT(OnMenuLoadImage()));
    connect(GetHotkey("Main Window", "Start Emulation", this), SIGNAL(activated()), this, SLOT(OnStartGame()));

    show();
}

GMainWindow::~GMainWindow()
{
    // will get automatically deleted otherwise
    if (render_window->parent() == NULL)
        delete render_window;
}

void GMainWindow::BootGame(const char* filename)
{
    render_window->DoneCurrent(); // make sure EmuThread can access GL context
    render_window->GetEmuThread().SetFilename(filename);
    render_window->GetEmuThread().start();

    SetupEmuWindowMode();
    render_window->show();
}

void GMainWindow::OnMenuLoadELF()
{
    QString filename = QFileDialog::getOpenFileName(this, tr("Load ELF"), QString(), QString());
    if (filename.size())
       BootGame(filename.toLatin1().data());
}

void GMainWindow::OnStartGame()
{
}

void GMainWindow::OnPauseGame()
{
}

void GMainWindow::OnStopGame()
{
}

void GMainWindow::OnOpenHotkeysDialog()
{
    GHotkeysDialog dialog(this);
    dialog.exec();
}


void GMainWindow::SetupEmuWindowMode()
{
    if (!render_window->GetEmuThread().isRunning())
        return;

    bool enable = ui.action_Single_Window_Mode->isChecked();
    if (enable && render_window->parent() == NULL) // switch to single window mode
    {
        render_window->BackupGeometry();
        ui.horizontalLayout->addWidget(render_window);
        render_window->setVisible(true);
        render_window->DoneCurrent();
    }
    else if (!enable && render_window->parent() != NULL) // switch to multiple windows mode
    {
        ui.horizontalLayout->removeWidget(render_window);
        render_window->setParent(NULL);
        render_window->setVisible(true);
        render_window->DoneCurrent();
        render_window->RestoreGeometry();
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
