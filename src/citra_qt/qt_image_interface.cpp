// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <QBuffer>
#include <QImage>
#include <QString>
#include "common/logging/log.h"
#include "core/frontend/image_interface.h"
#include "qt_image_interface.h"

bool QtImageInterface::DecodePNG(std::vector<u8>& dst, u32& width, u32& height,
                                 const std::string& path) {
    QImage image(QString::fromStdString(path));

    if (image.isNull()) {
        LOG_ERROR(Frontend, "Failed to open {} for decoding", path);
        return false;
    }
    width = image.width();
    height = image.height();

    // Write RGBA8 to vector
    for (u32 y = 1; y < image.height() + 1; y++) {
        for (u32 x = 1; x < image.width() + 1; x++) {
            const QColor pixel(image.pixel(y, x));
            dst.push_back(pixel.red());
            dst.push_back(pixel.green());
            dst.push_back(pixel.blue());
            dst.push_back(pixel.alpha());
        }
    }

    return true;
}

bool QtImageInterface::EncodePNG(const std::string& path, const std::vector<u8>& src, u32 width,
                                 u32 height) {
    QImage image(src.data(), width, height, QImage::Format_RGBA8888);

    if (!image.save(QString::fromStdString(path))) {
        LOG_ERROR(Frontend, "Failed to save {}", path);
        return false;
    }
    return true;
}