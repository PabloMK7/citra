// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <lodepng.h>
#include "citra/lodepng_image_interface.h"
#include "common/logging/log.h"

bool LodePNGImageInterface::DecodePNG(std::vector<u8>& dst, u32& width, u32& height,
                                      const std::string& path) {
    u32 lodepng_ret = lodepng::decode(dst, width, height, path);
    if (lodepng_ret) {
        LOG_CRITICAL(Frontend, "Failed to decode {} because {}", path,
                     lodepng_error_text(lodepng_ret));
        return false;
    }
    return true;
}

bool LodePNGImageInterface::EncodePNG(const std::string& path, const std::vector<u8>& src,
                                      u32 width, u32 height) {
    u32 lodepng_ret = lodepng::encode(path, src, width, height);
    if (lodepng_ret) {
        LOG_CRITICAL(Frontend, "Failed to encode {} because {}", path,
                     lodepng_error_text(lodepng_ret));
        return false;
    }
    return true;
}
