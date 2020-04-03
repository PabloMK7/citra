// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <memory>
#include <string_view>
#include <vector>
#include <glad/glad.h>
#include "common/common_types.h"
#include "common/math_util.h"
#include "video_core/renderer_opengl/gl_surface_params.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_base.h"

namespace OpenGL {

class TextureFilterer {
public:
    static constexpr std::string_view NONE = "none";

    explicit TextureFilterer(std::string_view filter_name, u16 scale_factor);
    // returns true if the filter actually changed
    bool Reset(std::string_view new_filter_name, u16 new_scale_factor);
    // returns true if there is no active filter
    bool IsNull() const;
    // returns true if the texture was able to be filtered
    bool Filter(GLuint src_tex, const Common::Rectangle<u32>& src_rect, GLuint dst_tex,
                const Common::Rectangle<u32>& dst_rect, SurfaceParams::SurfaceType type,
                GLuint read_fb_handle, GLuint draw_fb_handle);

    static std::vector<std::string_view> GetFilterNames();

private:
    std::string_view filter_name = NONE;
    std::unique_ptr<TextureFilterBase> filter;
};

} // namespace OpenGL
