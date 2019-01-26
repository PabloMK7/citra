// Copyright 2018 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cstring>
#include "core/dumping/backend.h"

namespace VideoDumper {

VideoFrame::VideoFrame(std::size_t width_, std::size_t height_, u8* data_)
    : width(width_), height(height_), stride(width * 4), data(width * height * 4) {
    // While copying, rotate the image to put the pixels in correct order
    // (As OpenGL returns pixel data starting from the lowest position)
    for (std::size_t i = 0; i < height; i++) {
        for (std::size_t j = 0; j < width; j++) {
            for (std::size_t k = 0; k < 4; k++) {
                data[i * stride + j * 4 + k] = data_[(height - i - 1) * stride + j * 4 + k];
            }
        }
    }
}

Backend::~Backend() = default;
NullBackend::~NullBackend() = default;

} // namespace VideoDumper