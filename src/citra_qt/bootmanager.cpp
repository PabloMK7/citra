#include <QHBoxLayout>
#include <QKeyEvent>
#include <QApplication>

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
// Required for screen DPI information
#include <QScreen>
#include <QWindow>
#endif

#include "bootmanager.h"
#include "main.h"

#include "core/core.h"
#include "core/settings.h"
#include "core/system.h"

#include "video_core/debug_utils/debug_utils.h"

#include "video_core/video_core.h"

#include "citra_qt/version.h"

#define APP_NAME        "citra"
#define APP_VERSION     "0.1-" VERSION
#define APP_TITLE       APP_NAME " " APP_VERSION
#define COPYRIGHT       "Copyright (C) 2013-2014 Citra Team"

EmuThread::EmuThread(GRenderWindow* render_window) :
    exec_step(false), running(false), stop_run(false), render_window(render_window) {

    connect(this, SIGNAL(started()), render_window, SLOT(moveContext()));
}

void EmuThread::run() {
    stop_run = false;

    // holds whether the cpu was running during the last iteration,
    // so that the DebugModeLeft signal can be emitted before the
    // next execution step
    bool was_active = false;
    while (!stop_run) {
        if (running) {
            if (!was_active)
                emit DebugModeLeft();

            Core::RunLoop();

            was_active = running || exec_step;
            if (!was_active && !stop_run)
                emit DebugModeEntered();
        } else if (exec_step) {
            if (!was_active)
                emit DebugModeLeft();

            exec_step = false;
            Core::SingleStep();
            emit DebugModeEntered();
            yieldCurrentThread();
            
            was_active = false;
        }
    }

    render_window->moveContext();
}

// This class overrides paintEvent and resizeEvent to prevent the GUI thread from stealing GL context.
// The corresponding functionality is handled in EmuThread instead
class GGLWidgetInternal : public QGLWidget
{
public:
    GGLWidgetInternal(QGLFormat fmt, GRenderWindow* parent)
                     : QGLWidget(fmt, parent), parent(parent) {
    }

    void paintEvent(QPaintEvent* ev) override {
    }

    void resizeEvent(QResizeEvent* ev) override {
        parent->OnClientAreaResized(ev->size().width(), ev->size().height());
        parent->OnFramebufferSizeChanged();
    }

private:
    GRenderWindow* parent;
};

GRenderWindow::GRenderWindow(QWidget* parent, EmuThread* emu_thread) :
    QWidget(parent), emu_thread(emu_thread), keyboard_id(0) {

    std::string window_title = Common::StringFromFormat("Citra | %s-%s", Common::g_scm_branch, Common::g_scm_desc);
    setWindowTitle(QString::fromStdString(window_title));

    keyboard_id = KeyMap::NewDeviceId();
    ReloadSetKeymaps();

    // TODO: One of these flags might be interesting: WA_OpaquePaintEvent, WA_NoBackground, WA_DontShowOnScreen, WA_DeleteOnClose
    QGLFormat fmt;
    fmt.setVersion(3,2);
    fmt.setProfile(QGLFormat::CoreProfile);
    // Requests a forward-compatible context, which is required to get a 3.2+ context on OS X
    fmt.setOption(QGL::NoDeprecatedFunctions);

    child = new GGLWidgetInternal(fmt, this);
    QBoxLayout* layout = new QHBoxLayout(this);

    resize(VideoCore::kScreenTopWidth, VideoCore::kScreenTopHeight + VideoCore::kScreenBottomHeight);
    layout->addWidget(child);
    layout->setMargin(0);
    setLayout(layout);

    OnMinimalClientAreaChangeRequest(GetActiveConfig().min_client_area_size);

    OnFramebufferSizeChanged();
    NotifyClientAreaSizeChanged(std::pair<unsigned,unsigned>(child->width(), child->height()));

    BackupGeometry();

#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    connect(this->windowHandle(), SIGNAL(screenChanged(QScreen*)), this, SLOT(OnFramebufferSizeChanged()));
#endif
}

void GRenderWindow::moveContext()
{
    DoneCurrent();
    // We need to move GL context to the swapping thread in Qt5
#if QT_VERSION > QT_VERSION_CHECK(5, 0, 0)
    // If the thread started running, move the GL Context to the new thread. Otherwise, move it back.
    auto thread = (QThread::currentThread() == qApp->thread() && emu_thread != nullptr) ? emu_thread : qApp->thread();
    child->context()->moveToThread(thread);
#endif
}

void GRenderWindow::SwapBuffers()
{
    // MakeCurrent is already called in renderer_opengl
    child->swapBuffers();
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
}

// On Qt 5.0+, this correctly gets the size of the framebuffer (pixels).
//
// Older versions get the window size (density independent pixels),
// and hence, do not support DPI scaling ("retina" displays).
// The result will be a viewport that is smaller than the extent of the window.
void GRenderWindow::OnFramebufferSizeChanged()
{
    // Screen changes potentially incur a change in screen DPI, hence we should update the framebuffer size
#if QT_VERSION >= QT_VERSION_CHECK(5, 0, 0)
    // windowHandle() might not be accessible until the window is displayed to screen.
    auto pixel_ratio = windowHandle() ? (windowHandle()->screen()->devicePixelRatio()) : 1.0;

    unsigned width = child->QPaintDevice::width() * pixel_ratio;
    unsigned height = child->QPaintDevice::height() * pixel_ratio;
#else
    unsigned width = child->QPaintDevice::width();
    unsigned height = child->QPaintDevice::height();
#endif

    NotifyFramebufferLayoutChanged(EmuWindow::FramebufferLayout::DefaultScreenLayout(width, height));
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
    if (parent() == nullptr)
        return ((QGLWidget*)this)->saveGeometry();
    else
        return geometry;
}

void GRenderWindow::keyPressEvent(QKeyEvent* event)
{
    this->KeyPressed({event->key(), keyboard_id});
}

void GRenderWindow::keyReleaseEvent(QKeyEvent* event)
{
    this->KeyReleased({event->key(), keyboard_id});
}

void GRenderWindow::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
    {
        auto pos = event->pos();
        this->TouchPressed(static_cast<unsigned>(pos.x()), static_cast<unsigned>(pos.y()));
    }
}

void GRenderWindow::mouseMoveEvent(QMouseEvent *event)
{
    auto pos = event->pos();
    this->TouchMoved(static_cast<unsigned>(std::max(pos.x(), 0)), static_cast<unsigned>(std::max(pos.y(), 0)));
}

void GRenderWindow::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton)
        this->TouchReleased();
}

void GRenderWindow::ReloadSetKeymaps()
{
    KeyMap::SetKeyMapping({Settings::values.pad_a_key,      keyboard_id}, Service::HID::PAD_A);
    KeyMap::SetKeyMapping({Settings::values.pad_b_key,      keyboard_id}, Service::HID::PAD_B);
    KeyMap::SetKeyMapping({Settings::values.pad_select_key, keyboard_id}, Service::HID::PAD_SELECT);
    KeyMap::SetKeyMapping({Settings::values.pad_start_key,  keyboard_id}, Service::HID::PAD_START);
    KeyMap::SetKeyMapping({Settings::values.pad_dright_key, keyboard_id}, Service::HID::PAD_RIGHT);
    KeyMap::SetKeyMapping({Settings::values.pad_dleft_key,  keyboard_id}, Service::HID::PAD_LEFT);
    KeyMap::SetKeyMapping({Settings::values.pad_dup_key,    keyboard_id}, Service::HID::PAD_UP);
    KeyMap::SetKeyMapping({Settings::values.pad_ddown_key,  keyboard_id}, Service::HID::PAD_DOWN);
    KeyMap::SetKeyMapping({Settings::values.pad_r_key,      keyboard_id}, Service::HID::PAD_R);
    KeyMap::SetKeyMapping({Settings::values.pad_l_key,      keyboard_id}, Service::HID::PAD_L);
    KeyMap::SetKeyMapping({Settings::values.pad_x_key,      keyboard_id}, Service::HID::PAD_X);
    KeyMap::SetKeyMapping({Settings::values.pad_y_key,      keyboard_id}, Service::HID::PAD_Y);

    KeyMap::SetKeyMapping({Settings::values.pad_zl_key,     keyboard_id}, Service::HID::PAD_ZL);
    KeyMap::SetKeyMapping({Settings::values.pad_zr_key,     keyboard_id}, Service::HID::PAD_ZR);

    // KeyMap::SetKeyMapping({Settings::values.pad_touch_key,  keyboard_id}, Service::HID::PAD_TOUCH);

    KeyMap::SetKeyMapping({Settings::values.pad_cright_key, keyboard_id}, Service::HID::PAD_C_RIGHT);
    KeyMap::SetKeyMapping({Settings::values.pad_cleft_key,  keyboard_id}, Service::HID::PAD_C_LEFT);
    KeyMap::SetKeyMapping({Settings::values.pad_cup_key,    keyboard_id}, Service::HID::PAD_C_UP);
    KeyMap::SetKeyMapping({Settings::values.pad_cdown_key,  keyboard_id}, Service::HID::PAD_C_DOWN);
    KeyMap::SetKeyMapping({Settings::values.pad_sright_key, keyboard_id}, Service::HID::PAD_CIRCLE_RIGHT);
    KeyMap::SetKeyMapping({Settings::values.pad_sleft_key,  keyboard_id}, Service::HID::PAD_CIRCLE_LEFT);
    KeyMap::SetKeyMapping({Settings::values.pad_sup_key,    keyboard_id}, Service::HID::PAD_CIRCLE_UP);
    KeyMap::SetKeyMapping({Settings::values.pad_sdown_key,  keyboard_id}, Service::HID::PAD_CIRCLE_DOWN);
}

void GRenderWindow::OnClientAreaResized(unsigned width, unsigned height)
{
    NotifyClientAreaSizeChanged(std::make_pair(width, height));
}

void GRenderWindow::OnMinimalClientAreaChangeRequest(const std::pair<unsigned,unsigned>& minimal_size) {
    setMinimumSize(minimal_size.first, minimal_size.second);
}

void GRenderWindow::OnEmulationStarting(EmuThread* emu_thread) {
    this->emu_thread = emu_thread;
}

void GRenderWindow::OnEmulationStopping() {
    emu_thread = nullptr;
}
