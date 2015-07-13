// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <atomic>
#include <condition_variable>
#include <mutex>

#include <QThread>
#include <QGLWidget>

#include "common/emu_window.h"
#include "common/thread.h"

class QScreen;
class QKeyEvent;

class GRenderWindow;
class GMainWindow;

class EmuThread : public QThread
{
    Q_OBJECT

public:
    EmuThread(GRenderWindow* render_window);

    /**
     * Start emulation (on new thread)
     * @warning Only call when not running!
     */
    void run() override;

    /**
     * Steps the emulation thread by a single CPU instruction (if the CPU is not already running)
     * @note This function is thread-safe
     */
    void ExecStep() {
        exec_step = true;
        running_cv.notify_all();
    }

    /**
     * Sets whether the emulation thread is running or not
     * @param running Boolean value, set the emulation thread to running if true
     * @note This function is thread-safe
     */
    void SetRunning(bool running) {
        std::unique_lock<std::mutex> lock(running_mutex);
        this->running = running;
        lock.unlock();
        running_cv.notify_all();
    }

    /**
     * Check if the emulation thread is running or not
     * @return True if the emulation thread is running, otherwise false
     * @note This function is thread-safe
     */
    bool IsRunning() { return running; }

    /**
     * Requests for the emulation thread to stop running
     */
    void RequestStop() {
        stop_run = true;
        SetRunning(false);
    };

private:
    bool exec_step;
    bool running;
    std::atomic<bool> stop_run;
    std::mutex running_mutex;
    std::condition_variable running_cv;

    GRenderWindow* render_window;

signals:
    /**
     * Emitted when the CPU has halted execution
     *
     * @warning When connecting to this signal from other threads, make sure to specify either Qt::QueuedConnection (invoke slot within the destination object's message thread) or even Qt::BlockingQueuedConnection (additionally block source thread until slot returns)
     */
    void DebugModeEntered();

    /**
     * Emitted right before the CPU continues execution
     *
     * @warning When connecting to this signal from other threads, make sure to specify either Qt::QueuedConnection (invoke slot within the destination object's message thread) or even Qt::BlockingQueuedConnection (additionally block source thread until slot returns)
     */
    void DebugModeLeft();
};

class GRenderWindow : public QWidget, public EmuWindow
{
    Q_OBJECT

public:
    GRenderWindow(QWidget* parent, EmuThread* emu_thread);

    // EmuWindow implementation
    void SwapBuffers() override;
    void MakeCurrent() override;
    void DoneCurrent() override;
    void PollEvents() override;

    void BackupGeometry();
    void RestoreGeometry();
    void restoreGeometry(const QByteArray& geometry); // overridden
    QByteArray saveGeometry();  // overridden

    void keyPressEvent(QKeyEvent* event) override;
    void keyReleaseEvent(QKeyEvent* event) override;

    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;

    void ReloadSetKeymaps() override;

    void OnClientAreaResized(unsigned width, unsigned height);

    void OnFramebufferSizeChanged();

public slots:
    void moveContext();  // overridden

    void OnEmulationStarting(EmuThread* emu_thread);
    void OnEmulationStopping();

private:
    void OnMinimalClientAreaChangeRequest(const std::pair<unsigned,unsigned>& minimal_size) override;

    QGLWidget* child;

    QByteArray geometry;

    /// Device id of keyboard for use with KeyMap
    int keyboard_id;

    EmuThread* emu_thread;
};
