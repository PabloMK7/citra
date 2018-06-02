// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <string>
#include <unordered_map>
#include <vector>
#include <QAbstractVideoSurface>
#include <QCamera>
#include <QCameraViewfinderSettings>
#include <QImage>
#include <QMutex>
#include "citra_qt/camera/camera_util.h"
#include "citra_qt/camera/qt_camera_base.h"
#include "core/frontend/camera/interface.h"

class GMainWindow;

namespace Camera {

class QtCameraSurface final : public QAbstractVideoSurface {
public:
    QList<QVideoFrame::PixelFormat> supportedPixelFormats(
        QAbstractVideoBuffer::HandleType) const override;
    bool present(const QVideoFrame&) override;

private:
    QMutex mutex;
    QImage current_frame;

    friend class QtMultimediaCamera; // For access to current_frame
};

class QtMultimediaCameraHandler;

/// This class is only an interface. It just calls QtMultimediaCameraHandler.
class QtMultimediaCamera final : public QtCameraInterface {
public:
    QtMultimediaCamera(const std::string& camera_name, const Service::CAM::Flip& flip);
    ~QtMultimediaCamera();
    void StartCapture() override;
    void StopCapture() override;
    void SetFrameRate(Service::CAM::FrameRate frame_rate) override;
    QImage QtReceiveFrame() override;
    bool IsPreviewAvailable() override;

private:
    std::shared_ptr<QtMultimediaCameraHandler> handler;
};

class QtMultimediaCameraFactory final : public QtCameraFactory {
public:
    std::unique_ptr<CameraInterface> Create(const std::string& config,
                                            const Service::CAM::Flip& flip) const override;
};

class QtMultimediaCameraHandler final : public QObject {
    Q_OBJECT

public:
    /// Creates the global handler. Must be called in UI thread.
    static void Init();
    static std::shared_ptr<QtMultimediaCameraHandler> GetHandler(const std::string& camera_name);
    static void ReleaseHandler(const std::shared_ptr<QtMultimediaCameraHandler>& handler);

    /**
     * Creates the camera.
     * Note: This function must be called via QMetaObject::invokeMethod in UI thread.
     */
    Q_INVOKABLE void CreateCamera(const std::string& camera_name);

    /**
     * Starts the camera.
     * Note: This function must be called via QMetaObject::invokeMethod in UI thread when
     *       starting the camera for the first time. 'Resume' calls can be in other threads.
     */
    Q_INVOKABLE void StartCamera();

    void StopCamera();
    bool CameraAvailable() const;
    static void StopCameras();
    static void ResumeCameras();
    static void ReleaseHandlers();

private:
    std::unique_ptr<QCamera> camera;
    QtCameraSurface camera_surface{};
    QCameraViewfinderSettings settings;
    bool started = false;

    static std::array<std::shared_ptr<QtMultimediaCameraHandler>, 3> handlers;
    static std::array<bool, 3> status;
    static std::unordered_map<std::string, std::shared_ptr<QtMultimediaCameraHandler>> loaded;

    friend class QtMultimediaCamera; // For access to camera_surface (and camera)
};

} // namespace Camera
