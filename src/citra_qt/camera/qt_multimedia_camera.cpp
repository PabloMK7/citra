// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QCamera>
#include <QCameraInfo>
#include <QImageReader>
#include <QMessageBox>
#include <QThread>
#include "citra_qt/camera/qt_multimedia_camera.h"
#include "citra_qt/main.h"

namespace Camera {

QList<QVideoFrame::PixelFormat> QtCameraSurface::supportedPixelFormats(
    QAbstractVideoBuffer::HandleType handleType) const {
    Q_UNUSED(handleType);
    return QList<QVideoFrame::PixelFormat>()
           << QVideoFrame::Format_ARGB32 << QVideoFrame::Format_ARGB32_Premultiplied
           << QVideoFrame::Format_RGB32 << QVideoFrame::Format_RGB24 << QVideoFrame::Format_RGB565
           << QVideoFrame::Format_RGB555 << QVideoFrame::Format_ARGB8565_Premultiplied
           << QVideoFrame::Format_BGRA32 << QVideoFrame::Format_BGRA32_Premultiplied
           << QVideoFrame::Format_BGR32 << QVideoFrame::Format_BGR24 << QVideoFrame::Format_BGR565
           << QVideoFrame::Format_BGR555 << QVideoFrame::Format_BGRA5658_Premultiplied
           << QVideoFrame::Format_AYUV444 << QVideoFrame::Format_AYUV444_Premultiplied
           << QVideoFrame::Format_YUV444 << QVideoFrame::Format_YUV420P << QVideoFrame::Format_YV12
           << QVideoFrame::Format_UYVY << QVideoFrame::Format_YUYV << QVideoFrame::Format_NV12
           << QVideoFrame::Format_NV21 << QVideoFrame::Format_IMC1 << QVideoFrame::Format_IMC2
           << QVideoFrame::Format_IMC3 << QVideoFrame::Format_IMC4 << QVideoFrame::Format_Y8
           << QVideoFrame::Format_Y16 << QVideoFrame::Format_Jpeg << QVideoFrame::Format_CameraRaw
           << QVideoFrame::Format_AdobeDng; // Supporting all the formats
}

bool QtCameraSurface::present(const QVideoFrame& frame) {
    if (!frame.isValid()) {
        return false;
    }
    QVideoFrame cloneFrame(frame);
    cloneFrame.map(QAbstractVideoBuffer::ReadOnly);
    const QImage image(cloneFrame.bits(), cloneFrame.width(), cloneFrame.height(),
                       QVideoFrame::imageFormatFromPixelFormat(cloneFrame.pixelFormat()));
    QMutexLocker locker(&mutex);
    current_frame = image.mirrored(true, true);
    locker.unlock();
    cloneFrame.unmap();
    return true;
}

QtMultimediaCamera::QtMultimediaCamera(const std::string& camera_name,
                                       const Service::CAM::Flip& flip)
    : QtCameraInterface(flip), handler(QtMultimediaCameraHandler::GetHandler(camera_name)) {
    if (handler->thread() == QThread::currentThread()) {
        handler->CreateCamera(camera_name);
    } else {
        QMetaObject::invokeMethod(handler.get(), "CreateCamera", Qt::BlockingQueuedConnection,
                                  Q_ARG(const std::string&, camera_name));
    }
}

QtMultimediaCamera::~QtMultimediaCamera() {
    handler->StopCamera();
    QtMultimediaCameraHandler::ReleaseHandler(handler);
}

void QtMultimediaCamera::StartCapture() {
    if (handler->thread() == QThread::currentThread()) {
        handler->StartCamera();
    } else {
        QMetaObject::invokeMethod(handler.get(), "StartCamera", Qt::BlockingQueuedConnection);
    }
}

void QtMultimediaCamera::StopCapture() {
    handler->StopCamera();
}

void QtMultimediaCamera::SetFrameRate(Service::CAM::FrameRate frame_rate) {
    const std::array<QCamera::FrameRateRange, 13> FrameRateList = {
        /* Rate_15 */ QCamera::FrameRateRange(15, 15),
        /* Rate_15_To_5 */ QCamera::FrameRateRange(5, 15),
        /* Rate_15_To_2 */ QCamera::FrameRateRange(2, 15),
        /* Rate_10 */ QCamera::FrameRateRange(10, 10),
        /* Rate_8_5 */ QCamera::FrameRateRange(8.5, 8.5),
        /* Rate_5 */ QCamera::FrameRateRange(5, 5),
        /* Rate_20 */ QCamera::FrameRateRange(20, 20),
        /* Rate_20_To_5 */ QCamera::FrameRateRange(5, 20),
        /* Rate_30 */ QCamera::FrameRateRange(30, 30),
        /* Rate_30_To_5 */ QCamera::FrameRateRange(5, 30),
        /* Rate_15_To_10 */ QCamera::FrameRateRange(10, 15),
        /* Rate_20_To_10 */ QCamera::FrameRateRange(10, 20),
        /* Rate_30_To_10 */ QCamera::FrameRateRange(10, 30),
    };

    auto framerate = FrameRateList[static_cast<int>(frame_rate)];

    if (handler->camera->supportedViewfinderFrameRateRanges().contains(framerate)) {
        handler->settings.setMinimumFrameRate(framerate.minimumFrameRate);
        handler->settings.setMaximumFrameRate(framerate.maximumFrameRate);
    }
}

QImage QtMultimediaCamera::QtReceiveFrame() {
    QMutexLocker locker(&handler->camera_surface.mutex);
    return handler->camera_surface.current_frame;
}

bool QtMultimediaCamera::IsPreviewAvailable() {
    return handler->CameraAvailable();
}

std::unique_ptr<CameraInterface> QtMultimediaCameraFactory::Create(const std::string& config,
                                                                   const Service::CAM::Flip& flip) {
    return std::make_unique<QtMultimediaCamera>(config, flip);
}

std::array<std::shared_ptr<QtMultimediaCameraHandler>, 3> QtMultimediaCameraHandler::handlers;

std::array<bool, 3> QtMultimediaCameraHandler::status;

std::unordered_map<std::string, std::shared_ptr<QtMultimediaCameraHandler>>
    QtMultimediaCameraHandler::loaded;

void QtMultimediaCameraHandler::Init() {
    for (auto& handler : handlers) {
        handler = std::make_shared<QtMultimediaCameraHandler>();
    }
}

std::shared_ptr<QtMultimediaCameraHandler> QtMultimediaCameraHandler::GetHandler(
    const std::string& camera_name) {
    if (loaded.count(camera_name)) {
        return loaded.at(camera_name);
    }
    for (int i = 0; i < handlers.size(); i++) {
        if (!status[i]) {
            LOG_INFO(Service_CAM, "Successfully got handler {}", i);
            status[i] = true;
            loaded.emplace(camera_name, handlers[i]);
            return handlers[i];
        }
    }
    LOG_CRITICAL(Service_CAM, "All handlers taken up");
    return nullptr;
}

void QtMultimediaCameraHandler::ReleaseHandler(
    const std::shared_ptr<Camera::QtMultimediaCameraHandler>& handler) {
    for (int i = 0; i < handlers.size(); i++) {
        if (handlers[i] == handler) {
            LOG_INFO(Service_CAM, "Successfully released handler {}", i);
            status[i] = false;
            handlers[i]->started = false;
            for (auto it = loaded.begin(); it != loaded.end(); it++) {
                if (it->second == handlers[i]) {
                    loaded.erase(it);
                    break;
                }
            }
            break;
        }
    }
}

void QtMultimediaCameraHandler::CreateCamera(const std::string& camera_name) {
    QList<QCameraInfo> cameras = QCameraInfo::availableCameras();
    for (const QCameraInfo& cameraInfo : cameras) {
        if (cameraInfo.deviceName().toStdString() == camera_name)
            camera = std::make_unique<QCamera>(cameraInfo);
    }
    if (!camera) { // no cameras found, using default camera
        camera = std::make_unique<QCamera>();
    }
    settings.setMinimumFrameRate(30);
    settings.setMaximumFrameRate(30);
    camera->setViewfinder(&camera_surface);
    camera->load();
    if (camera->supportedViewfinderPixelFormats().isEmpty()) {
        // The gstreamer plugin (used on linux systems) returns an empty list on querying supported
        // viewfinder pixel formats, and will not work without expliciting setting it to some value,
        // so we are defaulting to RGB565 here which should be fairly widely supported.
        settings.setPixelFormat(QVideoFrame::PixelFormat::Format_RGB565);
    }
}

void QtMultimediaCameraHandler::StopCamera() {
    camera->stop();
    started = false;
}

void QtMultimediaCameraHandler::StartCamera() {
    camera->setViewfinderSettings(settings);
    camera->start();
    started = true;
}

bool QtMultimediaCameraHandler::CameraAvailable() const {
    return camera && camera->isAvailable();
}

void QtMultimediaCameraHandler::StopCameras() {
    LOG_INFO(Service_CAM, "Stopping all cameras");
    for (auto& handler : handlers) {
        if (handler && handler->started) {
            handler->StopCamera();
        }
    }
}

void QtMultimediaCameraHandler::ResumeCameras() {
    for (auto& handler : handlers) {
        if (handler && handler->started) {
            handler->StartCamera();
        }
    }
}

void QtMultimediaCameraHandler::ReleaseHandlers() {
    StopCameras();
    LOG_INFO(Service_CAM, "Releasing all handlers");
    for (int i = 0; i < handlers.size(); i++) {
        status[i] = false;
        handlers[i]->started = false;
    }
}

} // namespace Camera
