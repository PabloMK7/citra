#include <QHBoxLayout>
#include <QKeyEvent>
#include <QApplication>

#include "common/common.h"
#include "bootmanager.hxx"

#include "core/core.h"
#include "core/loader/loader.h"
#include "core/hw/hw.h"

#include "video_core/video_core.h"

#include "version.h"

#define APP_NAME        "citra"
#define APP_VERSION     "0.1-" VERSION
#define APP_TITLE       APP_NAME " " APP_VERSION
#define COPYRIGHT       "Copyright (C) 2013-2014 Citra Team"

EmuThread::EmuThread(GRenderWindow* render_window) : 
    filename(""), exec_cpu_step(false), cpu_running(false),
    stop_run(false), render_window(render_window)
{
}

void EmuThread::SetFilename(std::string filename)
{
    this->filename = filename;
}

void EmuThread::run()
{
    stop_run = false;
    while (!stop_run)
    {
        for (int tight_loop = 0; tight_loop < 10000; ++tight_loop)
        {
            if (cpu_running || exec_cpu_step)
            {
                if (exec_cpu_step)
                    exec_cpu_step = false;

                Core::SingleStep();
                if (!cpu_running) {
                    emit CPUStepped();
                    yieldCurrentThread();
                }
            }
        }
    }
    render_window->moveContext();

    Core::Stop();
}

void EmuThread::Stop()
{
    if (!isRunning())
    {
        INFO_LOG(MASTER_LOG, "EmuThread::Stop called while emu thread wasn't running, returning...");
        return;
    }
    stop_run = true;

    //core::g_state = core::SYS_DIE;

    wait(500);
    if (isRunning())
    {
        WARN_LOG(MASTER_LOG, "EmuThread still running, terminating...");
        quit();
        wait(1000);
        if (isRunning())
        {
            WARN_LOG(MASTER_LOG, "EmuThread STILL running, something is wrong here...");
            terminate();
        }
    }
    INFO_LOG(MASTER_LOG, "EmuThread stopped");
}


// This class overrides paintEvent and resizeEvent to prevent the GUI thread from stealing GL context.
// The corresponding functionality is handled in EmuThread instead
class GGLWidgetInternal : public QGLWidget
{
public:
    GGLWidgetInternal(QGLFormat fmt, GRenderWindow* parent) : QGLWidget(fmt, parent)
    {
        parent_ = parent;
    }

    void paintEvent(QPaintEvent* ev)
    {
    }
    void resizeEvent(QResizeEvent* ev) {
        parent_->SetClientAreaWidth(size().width());
        parent_->SetClientAreaHeight(size().height());
    }
private:
    GRenderWindow* parent_;
};


EmuThread& GRenderWindow::GetEmuThread()
{
    return emu_thread;
}

static const std::pair<int, HID_User::PadState> default_key_map[] = {
    { Qt::Key_A, HID_User::PAD_A },
    { Qt::Key_B, HID_User::PAD_B },
    { Qt::Key_Backslash, HID_User::PAD_SELECT },
    { Qt::Key_Enter, HID_User::PAD_START },
    { Qt::Key_Right, HID_User::PAD_RIGHT },
    { Qt::Key_Left, HID_User::PAD_LEFT },
    { Qt::Key_Up, HID_User::PAD_UP },
    { Qt::Key_Down, HID_User::PAD_DOWN },
    { Qt::Key_R, HID_User::PAD_R },
    { Qt::Key_L, HID_User::PAD_L },
    { Qt::Key_X, HID_User::PAD_X },
    { Qt::Key_Y, HID_User::PAD_Y },
    { Qt::Key_H, HID_User::PAD_CIRCLE_RIGHT },
    { Qt::Key_F, HID_User::PAD_CIRCLE_LEFT },
    { Qt::Key_T, HID_User::PAD_CIRCLE_UP },
    { Qt::Key_G, HID_User::PAD_CIRCLE_DOWN },
};

GRenderWindow::GRenderWindow(QWidget* parent) : QWidget(parent), emu_thread(this)
{
    // Register a new ID for the default keyboard
    keyboard_id = KeyMap::NewDeviceId();

    // Set default key mappings for keyboard
    for (auto mapping : default_key_map) {
        KeyMap::SetKeyMapping({mapping.first, keyboard_id}, mapping.second);
    }

    // TODO: One of these flags might be interesting: WA_OpaquePaintEvent, WA_NoBackground, WA_DontShowOnScreen, WA_DeleteOnClose
    QGLFormat fmt;
    fmt.setProfile(QGLFormat::CoreProfile);
    fmt.setVersion(3,2);
    fmt.setSampleBuffers(true);
    fmt.setSamples(4);
    
    child = new GGLWidgetInternal(fmt, this);
    QBoxLayout* layout = new QHBoxLayout(this);
    resize(VideoCore::kScreenTopWidth, VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight);
    layout->addWidget(child);
    layout->setMargin(0);
    setLayout(layout);
    QObject::connect(&emu_thread, SIGNAL(started()), this, SLOT(moveContext()));

    BackupGeometry();
}

void GRenderWindow::moveContext()
{
    DoneCurrent();
    // We need to move GL context to the swapping thread in Qt5
#if QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
    // If the thread started running, move the GL Context to the new thread. Otherwise, move it back.
    child->context()->moveToThread((QThread::currentThread() == qApp->thread()) ? &emu_thread : qApp->thread());
#endif
}

GRenderWindow::~GRenderWindow()
{
    if (emu_thread.isRunning())
        emu_thread.Stop();
}

void GRenderWindow::SwapBuffers()
{
    // MakeCurrent is already called in renderer_opengl
    child->swapBuffers();
}

void GRenderWindow::closeEvent(QCloseEvent* event)
{
    if (emu_thread.isRunning())
        emu_thread.Stop();
    QWidget::closeEvent(event);
}

void GRenderWindow::MakeCurrent()
{
    child->makeCurrent();
}

void GRenderWindow::DoneCurrent()
{
    child->doneCurrent();
}

void GRenderWindow::PollEvents() {
    // TODO(ShizZy): Does this belong here? This is a reasonable place to update the window title
    //  from the main thread, but this should probably be in an event handler...
    /*
    static char title[128];
    sprintf(title, "%s (FPS: %02.02f)", window_title_.c_str(), 
        video_core::g_renderer->current_fps());
    setWindowTitle(title);
    */
}

void GRenderWindow::BackupGeometry()
{
    geometry = ((QGLWidget*)this)->saveGeometry();
}

void GRenderWindow::RestoreGeometry()
{
    // We don't want to back up the geometry here (obviously)
    QWidget::restoreGeometry(geometry);
}

void GRenderWindow::restoreGeometry(const QByteArray& geometry)
{
    // Make sure users of this class don't need to deal with backing up the geometry themselves
    QWidget::restoreGeometry(geometry);
    BackupGeometry();
}

QByteArray GRenderWindow::saveGeometry()
{
    // If we are a top-level widget, store the current geometry
    // otherwise, store the last backup
    if (parent() == NULL)
        return ((QGLWidget*)this)->saveGeometry();
    else
        return geometry;
}

void GRenderWindow::keyPressEvent(QKeyEvent* event)
{
    EmuWindow::KeyPressed({event->key(), keyboard_id});
    HID_User::PadUpdateComplete();
}

void GRenderWindow::keyReleaseEvent(QKeyEvent* event)
{
    EmuWindow::KeyReleased({event->key(), keyboard_id});
    HID_User::PadUpdateComplete();
}

