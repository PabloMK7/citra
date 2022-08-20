// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include <string_view>
#include "common/common_types.h"
#include "common/math_util.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {

class TextureRuntime;
class OGLTexture;

class TextureFilterBase {
    friend class TextureFilterer;
public:
    explicit TextureFilterBase(u16 scale_factor) : scale_factor(scale_factor) {
        draw_fbo.Create();
    };

    virtual ~TextureFilterBase() = default;

private:
    virtual void Filter(const OGLTexture& src_tex, Common::Rectangle<u32> src_rect,
                        const OGLTexture& dst_tex, Common::Rectangle<u32> dst_rect) = 0;

protected:
    OGLFramebuffer draw_fbo;
    const u16 scale_factor{};
};

} // namespace OpenGL
