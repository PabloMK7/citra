// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QMessageBox>
#include "citra_qt/camera/qt_camera_factory.h"

namespace Camera {

std::unique_ptr<CameraInterface> QtCameraFactory::CreatePreview(const std::string& config,
                                                                int width, int height) const {
    std::unique_ptr<CameraInterface> camera = Create(config);

    if (camera->IsPreviewAvailable()) {
        return camera;
    }
    QMessageBox::critical(
        nullptr, QObject::tr("Error"),
        (config.empty() ? QObject::tr("Couldn't load the camera")
                        : QObject::tr("Couldn't load %1").arg(QString::fromStdString(config))));
    return nullptr;
}

} // namespace Camera
