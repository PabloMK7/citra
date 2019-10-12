// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "core/frontend/image_interface.h"

class QtImageInterface final : public Frontend::ImageInterface {
public:
    bool DecodePNG(std::vector<u8>& dst, u32& width, u32& height, const std::string& path) override;
    bool EncodePNG(const std::string& path, const std::vector<u8>& src, u32 width,
                   u32 height) override;
};
