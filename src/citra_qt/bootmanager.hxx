#include <QThread>
#include <QGLWidget>
#include <atomic>
#include "common/common.h"
#include "common/emu_window.h"

class GRenderWindow;
class QKeyEvent;

class EmuThread : public QThread
{
    Q_OBJECT

public:
    /**
     * Set image filename
     * 
     * @param filename
     * @warning Only call when not running!
     */
    void SetFilename(std::string filename);

    /**
     * Start emulation (on new thread)
     *
     * @warning Only call when not running!
     */
    void run();

    /**
     * Allow the CPU to process a single instruction (if cpu is not running)
     *
     * @note This function is thread-safe
     */
    void ExecStep() { exec_cpu_step = true; }

    /**
     * Allow the CPU to continue processing instructions without interruption
     *
     * @note This function is thread-safe
     */
    void SetCpuRunning(bool running) { cpu_running = running; }

    /**
    * Allow the CPU to continue processing instructions without interruption
    *
    * @note This function is thread-safe
    */
    bool IsCpuRunning() { return cpu_running; }


public slots:
    /**
     * Stop emulation and wait for the thread to finish.
     *
     * @details: This function will wait a second for the thread to finish; if it hasn't finished until then, we'll terminate() it and wait another second, hoping that it will be terminated by then.
     * @note: This function is thread-safe.
     */
    void Stop();

private:
    friend class GRenderWindow;

    EmuThread(GRenderWindow* render_window);

    std::string filename;

    bool exec_cpu_step;
    bool cpu_running;
    std::atomic<bool> stop_run;

    GRenderWindow* render_window;

signals:
    /**
     * Emitted when CPU when we've finished processing a single Gekko instruction
     * 
     * @warning This will only be emitted when the CPU is not running (SetCpuRunning(false))
     * @warning When connecting to this signal from other threads, make sure to specify either Qt::QueuedConnection (invoke slot within the destination object's message thread) or even Qt::BlockingQueuedConnection (additionally block source thread until slot returns)
     */
    void CPUStepped();
};

class GRenderWindow : public QWidget, public EmuWindow
{
    Q_OBJECT

public:
    GRenderWindow(QWidget* parent = NULL);
    ~GRenderWindow();

    void closeEvent(QCloseEvent*);

    // EmuWindow implementation
    void SwapBuffers();
    void MakeCurrent();
    void DoneCurrent();
    void PollEvents();

    void BackupGeometry();
    void RestoreGeometry();
    void restoreGeometry(const QByteArray& geometry); // overridden
    QByteArray saveGeometry(); // overridden

    EmuThread& GetEmuThread();

    void keyPressEvent(QKeyEvent* event);
    void keyReleaseEvent(QKeyEvent* event);

    void ReloadSetKeymaps() override;

public slots:
    void moveContext();

private:
    QGLWidget* child;

    EmuThread emu_thread;

    QByteArray geometry;

    /// Device id of keyboard for use with KeyMap
    int keyboard_id;
};
