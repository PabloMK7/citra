// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <lodepng.h>
#include "common/file_util.h"
#include "common/logging/log.h"
#include "jni/lodepng_image_interface.h"

bool LodePNGImageInterface::DecodePNG(std::vector<u8>& dst, u32& width, u32& height,
                                      const std::string& path) {
    FileUtil::IOFile file(path, "rb");
    size_t read_size = file.GetSize();
    std::vector<u8> in(read_size);
    if (file.ReadBytes(&in[0], read_size) != read_size) {
        LOG_CRITICAL(Frontend, "Failed to decode {}", path);
    }
    u32 lodepng_ret = lodepng::decode(dst, width, height, in);
    if (lodepng_ret) {
        LOG_CRITICAL(Frontend, "Failed to decode {} because {}", path,
                     lodepng_error_text(lodepng_ret));
        return false;
    }
    return true;
}

bool LodePNGImageInterface::EncodePNG(const std::string& path, const std::vector<u8>& src,
                                      u32 width, u32 height) {
    std::vector<u8> out;
    u32 lodepng_ret = lodepng::encode(out, src, width, height);
    if (lodepng_ret) {
        LOG_CRITICAL(Frontend, "Failed to encode {} because {}", path,
                     lodepng_error_text(lodepng_ret));
        return false;
    }

    FileUtil::IOFile file(path, "wb");
    if (file.WriteBytes(&out[0], out.size()) != out.size()) {
        LOG_CRITICAL(Frontend, "Failed to save encode to path={}", path);
        return false;
    }

    return true;
}
