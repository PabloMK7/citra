#include <QApplication>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QOffScreenSurface>
#include <QOpenGLContext>
#include <QOpenGLFunctions>
#include <QScreen>
#include <QWindow>
#include <fmt/format.h>
#include "citra_qt/bootmanager.h"
#include "common/microprofile.h"
#include "common/scm_rev.h"
#include "core/3ds.h"
#include "core/core.h"
#include "core/frontend/scope_acquire_context.h"
#include "core/settings.h"
#include "input_common/keyboard.h"
#include "input_common/main.h"
#include "input_common/motion_emu.h"
#include "network/network.h"
#include "video_core/renderer_base.h"
#include "video_core/video_core.h"

EmuThread::EmuThread(Frontend::GraphicsContext& core_context) : core_context(core_context) {}

EmuThread::~EmuThread() = default;

void EmuThread::run() {
    MicroProfileOnThreadCreate("EmuThread");
    Frontend::ScopeAcquireContext scope(core_context);
    // Holds whether the cpu was running during the last iteration,
    // so that the DebugModeLeft signal can be emitted before the
    // next execution step.
    bool was_active = false;
    while (!stop_run) {
        if (running) {
            if (!was_active)
                emit DebugModeLeft();

            Core::System::ResultStatus result = Core::System::GetInstance().RunLoop();
            if (result == Core::System::ResultStatus::ShutdownRequested) {
                // Notify frontend we shutdown
                emit ErrorThrown(result, "");
                // End emulation execution
                break;
            }
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
            std::unique_lock lock{running_mutex};
            running_cv.wait(lock, [this] { return IsRunning() || exec_step || stop_run; });
        }
    }

    // Shutdown the core emulation
    Core::System::GetInstance().Shutdown();

#if MICROPROFILE_ENABLED
    MicroProfileOnThreadExit();
#endif
}

GRenderWindow::GRenderWindow(QWidget* parent, EmuThread* emu_thread)
    : QOpenGLWidget(parent), emu_thread(emu_thread) {

    setWindowTitle(QStringLiteral("Citra %1 | %2-%3")
                       .arg(Common::g_build_name, Common::g_scm_branch, Common::g_scm_desc));
    setAttribute(Qt::WA_AcceptTouchEvents);

    InputCommon::Init();
}

GRenderWindow::~GRenderWindow() {
    InputCommon::Shutdown();
}

void GRenderWindow::MakeCurrent() {
    core_context->MakeCurrent();
}

void GRenderWindow::DoneCurrent() {
    core_context->DoneCurrent();
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
    const qreal pixel_ratio = windowPixelRatio();
    const u32 width = this->width() * pixel_ratio;
    const u32 height = this->height() * pixel_ratio;
    UpdateCurrentFramebufferLayout(width, height);
}

void GRenderWindow::BackupGeometry() {
    geometry = QWidget::saveGeometry();
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
    if (parent() == nullptr) {
        return QWidget::saveGeometry();
    }

    return geometry;
}

qreal GRenderWindow::windowPixelRatio() const {
    // windowHandle() might not be accessible until the window is displayed to screen.
    return windowHandle() ? windowHandle()->screen()->devicePixelRatio() : 1.0f;
}

std::pair<u32, u32> GRenderWindow::ScaleTouch(const QPointF pos) const {
    const qreal pixel_ratio = windowPixelRatio();
    return {static_cast<u32>(std::max(std::round(pos.x() * pixel_ratio), qreal{0.0})),
            static_cast<u32>(std::max(std::round(pos.y() * pixel_ratio), qreal{0.0}))};
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
    if (event->source() == Qt::MouseEventSynthesizedBySystem)
        return; // touch input is handled in TouchBeginEvent

    auto pos = event->pos();
    if (event->button() == Qt::LeftButton) {
        const auto [x, y] = ScaleTouch(pos);
        this->TouchPressed(x, y);
    } else if (event->button() == Qt::RightButton) {
        InputCommon::GetMotionEmu()->BeginTilt(pos.x(), pos.y());
    }
}

void GRenderWindow::mouseMoveEvent(QMouseEvent* event) {
    if (event->source() == Qt::MouseEventSynthesizedBySystem)
        return; // touch input is handled in TouchUpdateEvent

    auto pos = event->pos();
    const auto [x, y] = ScaleTouch(pos);
    this->TouchMoved(x, y);
    InputCommon::GetMotionEmu()->Tilt(pos.x(), pos.y());
}

void GRenderWindow::mouseReleaseEvent(QMouseEvent* event) {
    if (event->source() == Qt::MouseEventSynthesizedBySystem)
        return; // touch input is handled in TouchEndEvent

    if (event->button() == Qt::LeftButton)
        this->TouchReleased();
    else if (event->button() == Qt::RightButton)
        InputCommon::GetMotionEmu()->EndTilt();
}

void GRenderWindow::TouchBeginEvent(const QTouchEvent* event) {
    // TouchBegin always has exactly one touch point, so take the .first()
    const auto [x, y] = ScaleTouch(event->touchPoints().first().pos());
    this->TouchPressed(x, y);
}

void GRenderWindow::TouchUpdateEvent(const QTouchEvent* event) {
    QPointF pos;
    int active_points = 0;

    // average all active touch points
    for (const auto tp : event->touchPoints()) {
        if (tp.state() & (Qt::TouchPointPressed | Qt::TouchPointMoved | Qt::TouchPointStationary)) {
            active_points++;
            pos += tp.pos();
        }
    }

    pos /= active_points;

    const auto [x, y] = ScaleTouch(pos);
    this->TouchMoved(x, y);
}

void GRenderWindow::TouchEndEvent() {
    this->TouchReleased();
}

bool GRenderWindow::event(QEvent* event) {
    if (event->type() == QEvent::TouchBegin) {
        TouchBeginEvent(static_cast<QTouchEvent*>(event));
        return true;
    } else if (event->type() == QEvent::TouchUpdate) {
        TouchUpdateEvent(static_cast<QTouchEvent*>(event));
        return true;
    } else if (event->type() == QEvent::TouchEnd || event->type() == QEvent::TouchCancel) {
        TouchEndEvent();
        return true;
    }

    return QWidget::event(event);
}

void GRenderWindow::focusOutEvent(QFocusEvent* event) {
    QWidget::focusOutEvent(event);
    InputCommon::GetKeyboard()->ReleaseAllKeys();
}

void GRenderWindow::resizeEvent(QResizeEvent* event) {
    QOpenGLWidget::resizeEvent(event);
    NotifyClientAreaSizeChanged(std::make_pair(event->size().width(), event->size().height()));
    OnFramebufferSizeChanged();
}

void GRenderWindow::InitRenderTarget() {
    // TODO: One of these flags might be interesting: WA_OpaquePaintEvent, WA_NoBackground,
    // WA_DontShowOnScreen, WA_DeleteOnClose
    core_context = CreateSharedContext();
    BackupGeometry();
}

void GRenderWindow::CaptureScreenshot(u32 res_scale, const QString& screenshot_path) {
    if (res_scale == 0)
        res_scale = VideoCore::GetResolutionScaleFactor();
    const Layout::FramebufferLayout layout{Layout::FrameLayoutFromResolutionScale(res_scale)};
    screenshot_image = QImage(QSize(layout.width, layout.height), QImage::Format_RGB32);
    VideoCore::RequestScreenshot(
        screenshot_image.bits(),
        [=] {
            const std::string std_screenshot_path = screenshot_path.toStdString();
            if (screenshot_image.mirrored(false, true).save(screenshot_path)) {
                LOG_INFO(Frontend, "Screenshot saved to \"{}\"", std_screenshot_path);
            } else {
                LOG_ERROR(Frontend, "Failed to save screenshot to \"{}\"", std_screenshot_path);
            }
        },
        layout);
}

void GRenderWindow::OnMinimalClientAreaChangeRequest(std::pair<u32, u32> minimal_size) {
    setMinimumSize(minimal_size.first, minimal_size.second);
}

void GRenderWindow::OnEmulationStarting(EmuThread* emu_thread) {
    this->emu_thread = emu_thread;
}

void GRenderWindow::OnEmulationStopping() {
    emu_thread = nullptr;
}

void GRenderWindow::paintGL() {
    VideoCore::g_renderer->Present();
    update();
}

void GRenderWindow::showEvent(QShowEvent* event) {
    QWidget::showEvent(event);

    // windowHandle() is not initialized until the Window is shown, so we connect it here.
    connect(windowHandle(), &QWindow::screenChanged, this, &GRenderWindow::OnFramebufferSizeChanged,
            Qt::UniqueConnection);
}

std::unique_ptr<Frontend::GraphicsContext> GRenderWindow::CreateSharedContext() const {
    return std::make_unique<GGLContext>(QOpenGLContext::globalShareContext());
}

GGLContext::GGLContext(QOpenGLContext* shared_context)
    : context(new QOpenGLContext(shared_context->parent())),
      surface(new QOffscreenSurface(nullptr, shared_context->parent())) {
    context->setShareContext(shared_context);
    context->create();
    surface->setFormat(shared_context->format());
    surface->create();
}

void GGLContext::MakeCurrent() {
    context->makeCurrent(surface);
}

void GGLContext::DoneCurrent() {
    context->doneCurrent();
}
