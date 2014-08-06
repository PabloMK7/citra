#include <QHBoxLayout>
#include <QKeyEvent>

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

EmuThread::EmuThread(GRenderWindow* render_window) : exec_cpu_step(false), cpu_running(false), render_window(render_window)
{
}

void EmuThread::SetFilename(const char* filename)
{
    strcpy(this->filename, filename);
}

void EmuThread::run()
{
    while (true)
    {
        for (int tight_loop = 0; tight_loop < 10000; ++tight_loop)
        {
            if (cpu_running || exec_cpu_step)
            {
                if (exec_cpu_step)
                    exec_cpu_step = false;

                Core::SingleStep();
                if (!cpu_running)
                    emit CPUStepped();
            }
        }
    }

    Core::Stop();
}

void EmuThread::Stop()
{
    if (!isRunning())
    {
        INFO_LOG(MASTER_LOG, "EmuThread::Stop called while emu thread wasn't running, returning...");
        return;
    }

    //core::g_state = core::SYS_DIE;

    wait(1000);
    if (isRunning())
    {
        WARN_LOG(MASTER_LOG, "EmuThread still running, terminating...");
        terminate();
        wait(1000);
        if (isRunning())
            WARN_LOG(MASTER_LOG, "EmuThread STILL running, something is wrong here...");
    }
    INFO_LOG(MASTER_LOG, "EmuThread stopped");
}


// This class overrides paintEvent and resizeEvent to prevent the GUI thread from stealing GL context.
// The corresponding functionality is handled in EmuThread instead
class GGLWidgetInternal : public QGLWidget
{
public:
    GGLWidgetInternal(QGLFormat fmt, GRenderWindow* parent) : QGLWidget(parent)
    {
        doneCurrent();
        parent_ = parent;
    }

    void paintEvent(QPaintEvent* ev)
    {
        // Apparently, Windows doesn't display anything if we don't call this here.
        // TODO: Breaks linux though because we aren't calling doneCurrent() ... -.-
//        makeCurrent();
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

GRenderWindow::GRenderWindow(QWidget* parent) : QWidget(parent), emu_thread(this)
{
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

    BackupGeometry();
}

GRenderWindow::~GRenderWindow()
{
    emu_thread.Stop();
}

void GRenderWindow::SwapBuffers()
{
    child->makeCurrent(); // TODO: Not necessary?
    child->swapBuffers();
}

void GRenderWindow::closeEvent(QCloseEvent* event)
{
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
    /*
    bool key_processed = false;
    for (unsigned int channel = 0; channel < 4 && controller_interface(); ++channel)
        if (controller_interface()->SetControllerStatus(channel, event->key(), input_common::GCController::PRESSED))
            key_processed = true;

    if (!key_processed)
        QWidget::keyPressEvent(event);
    */
}

void GRenderWindow::keyReleaseEvent(QKeyEvent* event)
{
    /*
    bool key_processed = false;
    for (unsigned int channel = 0; channel < 4 && controller_interface(); ++channel)
        if (controller_interface()->SetControllerStatus(channel, event->key(), input_common::GCController::RELEASED))
            key_processed = true;

    if (!key_processed)
        QWidget::keyPressEvent(event);
    */
}