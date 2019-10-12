// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QImage>
#include <QString>
#include "citra_qt/qt_image_interface.h"
#include "common/logging/log.h"

bool QtImageInterface::DecodePNG(std::vector<u8>& dst, u32& width, u32& height,
                                 const std::string& path) {
    QImage image(QString::fromStdString(path));

    if (image.isNull()) {
        LOG_ERROR(Frontend, "Failed to open {} for decoding", path);
        return false;
    }
    width = image.width();
    height = image.height();

    image = image.convertToFormat(QImage::Format_RGBA8888);

    // Write RGBA8 to vector
    dst = std::vector<u8>(image.constBits(), image.constBits() + (width * height * 4));

    return true;
}

bool QtImageInterface::EncodePNG(const std::string& path, const std::vector<u8>& src, u32 width,
                                 u32 height) {
    QImage image(src.data(), width, height, QImage::Format_RGBA8888);

    if (!image.save(QString::fromStdString(path), "PNG")) {
        LOG_ERROR(Frontend, "Failed to save {}", path);
        return false;
    }
    return true;
}
