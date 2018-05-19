// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QFileDialog>
#include <QImageReader>
#include <QMessageBox>
#include "citra_qt/camera/still_image_camera.h"

namespace Camera {

StillImageCamera::StillImageCamera(QImage image_) : image(std::move(image_)) {}

void StillImageCamera::StartCapture() {}

void StillImageCamera::StopCapture() {}

void StillImageCamera::SetFormat(Service::CAM::OutputFormat output_format) {
    output_rgb = output_format == Service::CAM::OutputFormat::RGB565;
}

void StillImageCamera::SetResolution(const Service::CAM::Resolution& resolution) {
    width = resolution.width;
    height = resolution.height;
}

void StillImageCamera::SetFlip(Service::CAM::Flip flip) {
    using namespace Service::CAM;
    flip_horizontal = (flip == Flip::Horizontal) || (flip == Flip::Reverse);
    flip_vertical = (flip == Flip::Vertical) || (flip == Flip::Reverse);
}

void StillImageCamera::SetEffect(Service::CAM::Effect effect) {
    if (effect != Service::CAM::Effect::None) {
        NGLOG_ERROR(Service_CAM, "Unimplemented effect {}", static_cast<int>(effect));
    }
}

std::vector<u16> StillImageCamera::ReceiveFrame() {
    return CameraUtil::ProcessImage(image, width, height, output_rgb, flip_horizontal,
                                    flip_vertical);
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

std::unique_ptr<CameraInterface> StillImageCameraFactory::Create(const std::string& config) const {
    std::string real_config = config;
    if (config.empty()) {
        real_config = getFilePath();
    }
    QImage image(QString::fromStdString(real_config));
    if (image.isNull()) {
        NGLOG_ERROR(Service_CAM, "Couldn't load image \"{}\"", real_config.c_str());
    }
    return std::make_unique<StillImageCamera>(image);
}

} // namespace Camera
