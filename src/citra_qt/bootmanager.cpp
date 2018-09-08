#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QScreen>
#include <QWindow>
#include <fmt/format.h>

#include "citra_qt/bootmanager.h"
#include "common/microprofile.h"
#include "common/scm_rev.h"
#include "core/3ds.h"
#include "core/core.h"
#include "core/settings.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_emu.h"
#include "network/network.h"

EmuThread::EmuThread(GRenderWindow* render_window) : render_window(render_window) {}

void EmuThread::run() {
    render_window->MakeCurrent();

    MicroProfileOnThreadCreate("EmuThread");

    stop_run = false;

    // Holds whether the cpu was running during the last iteration,
    // so that the DebugModeLeft signal can be emitted before the
    // next execution step.
    bool was_active = false;
    while (!stop_run) {
        if (running) {
            if (!was_active)
                emit DebugModeLeft();

            Core::System::ResultStatus result = Core::System::GetInstance().RunLoop();
            if (result != Core::System::ResultStatus::Success) {
                this->SetRunning(false);
                emit ErrorThrown(result, Core::System::GetInstance().GetStatusDetails());
            }

            was_active = running || exec_step;
            if (!was_active && !stop_run)
                emit DebugModeEntered();
        } else if (exec_step) {
            if (!was_active)
                emit DebugModeLeft();

            exec_step = false;
            Core::System::GetInstance().SingleStep();
            emit DebugModeEntered();
            yieldCurrentThread();

            was_active = false;
        } else {
            std::unique_lock<std::mutex> lock(running_mutex);
            running_cv.wait(lock, [this] { return IsRunning() || exec_step || stop_run; });
        }
    }

    // Shutdown the core emulation
    Core::System::GetInstance().Shutdown();

#if MICROPROFILE_ENABLED
    MicroProfileOnThreadExit();
#endif

    render_window->moveContext();
}

// This class overrides paintEvent and resizeEvent to prevent the GUI thread from stealing GL
// context.
// The corresponding functionality is handled in EmuThread instead
class GGLWidgetInternal : public QGLWidget {
public:
    GGLWidgetInternal(QGLFormat fmt, GRenderWindow* parent)
        : QGLWidget(fmt, parent), parent(parent) {}

    void paintEvent(QPaintEvent* ev) override {
        if (do_painting) {
            QPainter painter(this);
        }
    }

    void resizeEvent(QResizeEvent* ev) override {
        parent->OnClientAreaResized(ev->size().width(), ev->size().height());
        parent->OnFramebufferSizeChanged();
    }

    void DisablePainting() {
        do_painting = false;
    }
    void EnablePainting() {
        do_painting = true;
    }

private:
    GRenderWindow* parent;
    bool do_painting;
};

GRenderWindow::GRenderWindow(QWidget* parent, EmuThread* emu_thread)
    : QWidget(parent), child(nullptr), emu_thread(emu_thread) {

    std::string window_title = fmt::format("Citra {} | {}-{}", Common::g_build_name,
                                           Common::g_scm_branch, Common::g_scm_desc);
    setWindowTitle(QString::fromStdString(window_title));

    InputCommon::Init();
    InputCommon::StartJoystickEventHandler();
}

GRenderWindow::~GRenderWindow() {
    InputCommon::Shutdown();
}

void GRenderWindow::moveContext() {
    DoneCurrent();

    // If the thread started running, move the GL Context to the new thread. Otherwise, move it
    // back.
    auto thread = (QThread::currentThread() == qApp->thread() && emu_thread != nullptr)
                      ? emu_thread
                      : qApp->thread();
    child->context()->moveToThread(thread);
}

void GRenderWindow::SwapBuffers() {
    // In our multi-threaded QGLWidget use case we shouldn't need to call `makeCurrent`,
    // since we never call `doneCurrent` in this thread.
    // However:
    // - The Qt debug runtime prints a bogus warning on the console if `makeCurrent` wasn't called
    // since the last time `swapBuffers` was executed;
    // - On macOS, if `makeCurrent` isn't called explicitely, resizing the buffer breaks.
    child->makeCurrent();

    child->swapBuffers();
}

void GRenderWindow::MakeCurrent() {
    child->makeCurrent();
}

void GRenderWindow::DoneCurrent() {
    child->doneCurrent();
}

void GRenderWindow::PollEvents() {}

// On Qt 5.0+, this correctly gets the size of the framebuffer (pixels).
//
// Older versions get the window size (density independent pixels),
// and hence, do not support DPI scaling ("retina" displays).
// The result will be a viewport that is smaller than the extent of the window.
void GRenderWindow::OnFramebufferSizeChanged() {
    // Screen changes potentially incur a change in screen DPI, hence we should update the
    // framebuffer size
    qreal pixelRatio = windowPixelRatio();
    unsigned width = child->QPaintDevice::width() * pixelRatio;
    unsigned height = child->QPaintDevice::height() * pixelRatio;
    UpdateCurrentFramebufferLayout(width, height);
}

void GRenderWindow::BackupGeometry() {
    geometry = ((QGLWidget*)this)->saveGeometry();
}

void GRenderWindow::RestoreGeometry() {
    // We don't want to back up the geometry here (obviously)
    QWidget::restoreGeometry(geometry);
}

void GRenderWindow::restoreGeometry(const QByteArray& geometry) {
    // Make sure users of this class don't need to deal with backing up the geometry themselves
    QWidget::restoreGeometry(geometry);
    BackupGeometry();
}

QByteArray GRenderWindow::saveGeometry() {
    // If we are a top-level widget, store the current geometry
    // otherwise, store the last backup
    if (parent() == nullptr)
        return ((QGLWidget*)this)->saveGeometry();
    else
        return geometry;
}

qreal GRenderWindow::windowPixelRatio() {
    // windowHandle() might not be accessible until the window is displayed to screen.
    return windowHandle() ? windowHandle()->screen()->devicePixelRatio() : 1.0f;
}

void GRenderWindow::closeEvent(QCloseEvent* event) {
    emit Closed();
    QWidget::closeEvent(event);
}

void GRenderWindow::keyPressEvent(QKeyEvent* event) {
    InputCommon::GetKeyboard()->PressKey(event->key());
}

void GRenderWindow::keyReleaseEvent(QKeyEvent* event) {
    InputCommon::GetKeyboard()->ReleaseKey(event->key());
}

void GRenderWindow::mousePressEvent(QMouseEvent* event) {
    auto pos = event->pos();
    if (event->button() == Qt::LeftButton) {
        qreal pixelRatio = windowPixelRatio();
        this->TouchPressed(static_cast<unsigned>(pos.x() * pixelRatio),
                           static_cast<unsigned>(pos.y() * pixelRatio));
    } else if (event->button() == Qt::RightButton) {
        InputCommon::GetMotionEmu()->BeginTilt(pos.x(), pos.y());
    }
}

void GRenderWindow::mouseMoveEvent(QMouseEvent* event) {
    auto pos = event->pos();
    qreal pixelRatio = windowPixelRatio();
    this->TouchMoved(std::max(static_cast<unsigned>(pos.x() * pixelRatio), 0u),
                     std::max(static_cast<unsigned>(pos.y() * pixelRatio), 0u));
    InputCommon::GetMotionEmu()->Tilt(pos.x(), pos.y());
}

void GRenderWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->button() == Qt::LeftButton)
        this->TouchReleased();
    else if (event->button() == Qt::RightButton)
        InputCommon::GetMotionEmu()->EndTilt();
}

void GRenderWindow::focusOutEvent(QFocusEvent* event) {
    QWidget::focusOutEvent(event);
    InputCommon::GetKeyboard()->ReleaseAllKeys();
}

void GRenderWindow::OnClientAreaResized(unsigned width, unsigned height) {
    NotifyClientAreaSizeChanged(std::make_pair(width, height));
}

void GRenderWindow::InitRenderTarget() {
    if (child) {
        delete child;
    }

    if (layout()) {
        delete layout();
    }

    // TODO: One of these flags might be interesting: WA_OpaquePaintEvent, WA_NoBackground,
    // WA_DontShowOnScreen, WA_DeleteOnClose
    QGLFormat fmt;
    fmt.setVersion(3, 3);
    fmt.setProfile(QGLFormat::CoreProfile);
    fmt.setSwapInterval(Settings::values.use_vsync);

    // Requests a forward-compatible context, which is required to get a 3.2+ context on OS X
    fmt.setOption(QGL::NoDeprecatedFunctions);

    child = new GGLWidgetInternal(fmt, this);
    QBoxLayout* layout = new QHBoxLayout(this);

    resize(Core::kScreenTopWidth, Core::kScreenTopHeight + Core::kScreenBottomHeight);
    layout->addWidget(child);
    layout->setMargin(0);
    setLayout(layout);

    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);

    OnFramebufferSizeChanged();
    NotifyClientAreaSizeChanged(std::pair<unsigned, unsigned>(child->width(), child->height()));

    BackupGeometry();
}

void GRenderWindow::OnMinimalClientAreaChangeRequest(
    const std::pair<unsigned, unsigned>& minimal_size) {
    setMinimumSize(minimal_size.first, minimal_size.second);
}

void GRenderWindow::OnEmulationStarting(EmuThread* emu_thread) {
    this->emu_thread = emu_thread;
    child->DisablePainting();
}

void GRenderWindow::OnEmulationStopping() {
    emu_thread = nullptr;
    child->EnablePainting();
}

void GRenderWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);

    // windowHandle() is not initialized until the Window is shown, so we connect it here.
    connect(windowHandle(), &QWindow::screenChanged, this, &GRenderWindow::OnFramebufferSizeChanged,
            Qt::UniqueConnection);
}
