// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QFileDialog>
#include <QImageReader>
#include <QMessageBox>
#include "citra_qt/camera/still_image_camera.h"

namespace Camera {

StillImageCamera::StillImageCamera(QImage image_, const Service::CAM::Flip& flip)
    : QtCameraInterface(flip), image(std::move(image_)) {}

void StillImageCamera::StartCapture() {}

void StillImageCamera::StopCapture() {}

QImage StillImageCamera::QtReceiveFrame() {
    return image;
}

bool StillImageCamera::IsPreviewAvailable() {
    return !image.isNull();
}

const std::string StillImageCameraFactory::getFilePath() {
    QList<QByteArray> types = QImageReader::supportedImageFormats();
    QList<QString> temp_filters;
    for (QByteArray type : types) {
        temp_filters << QString("*." + QString(type));
    }

    QString filter = QObject::tr("Supported image files (%1)").arg(temp_filters.join(" "));

    return QFileDialog::getOpenFileName(nullptr, QObject::tr("Open File"), ".", filter)
        .toStdString();
}

std::unique_ptr<CameraInterface> StillImageCameraFactory::Create(
    const std::string& config, const Service::CAM::Flip& flip) const {
    std::string real_config = config;
    if (config.empty()) {
        real_config = getFilePath();
    }
    QImage image(QString::fromStdString(real_config));
    if (image.isNull()) {
        NGLOG_ERROR(Service_CAM, "Couldn't load image \"{}\"", real_config.c_str());
    }
    return std::make_unique<StillImageCamera>(image, flip);
}

} // namespace Camera
