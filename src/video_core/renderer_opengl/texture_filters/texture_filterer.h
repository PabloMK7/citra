// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string_view>
#include <vector>
#include "video_core/rasterizer_cache/pixel_format.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_base.h"

namespace OpenGL {

class TextureFilterer {
public:
    static constexpr std::string_view NONE = "none";

public:
    explicit TextureFilterer(std::string_view filter_name, u16 scale_factor);

    // Returns true if the filter actually changed
    bool Reset(std::string_view new_filter_name, u16 new_scale_factor);

    // Returns true if there is no active filter
    bool IsNull() const;

    // Returns true if the texture was able to be filtered
    bool Filter(const OGLTexture& src_tex, Common::Rectangle<u32> src_rect,
                const OGLTexture& dst_tex, Common::Rectangle<u32> dst_rect,
                SurfaceType type);

    static std::vector<std::string_view> GetFilterNames();

private:
    std::string_view filter_name = NONE;
    std::unique_ptr<TextureFilterBase> filter;
};

} // namespace OpenGL
