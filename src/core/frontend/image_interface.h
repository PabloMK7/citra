// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
#include <vector>
#include "common/common_types.h"

namespace Frontend {

class ImageInterface {
public:
    virtual ~ImageInterface() = default;

    // Error logging should be handled by the frontend
    virtual bool DecodePNG(std::vector<u8>& dst, u32& width, u32& height,
                           const std::string& path) = 0;
    virtual bool EncodePNG(const std::string& path, const std::vector<u8>& src, u32 width,
                           u32 height) = 0;
};

} // namespace Frontend
