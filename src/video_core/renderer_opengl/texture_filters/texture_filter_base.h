// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/common_types.h"
#include "common/math_util.h"
#include "video_core/renderer_opengl/gl_surface_params.h"

namespace OpenGL {

class TextureFilterBase {
    friend class TextureFilterer;
    virtual void Filter(GLuint src_tex, const Common::Rectangle<u32>& src_rect, GLuint dst_tex,
                        const Common::Rectangle<u32>& dst_rect, GLuint read_fb_handle,
                        GLuint draw_fb_handle) = 0;

public:
    explicit TextureFilterBase(u16 scale_factor) : scale_factor{scale_factor} {};
    virtual ~TextureFilterBase() = default;

    const u16 scale_factor{};
};

} // namespace OpenGL
