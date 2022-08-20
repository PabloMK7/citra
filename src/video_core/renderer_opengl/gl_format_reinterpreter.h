// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include "common/math_util.h"
#include "video_core/rasterizer_cache/pixel_format.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {

class RasterizerCacheOpenGL;

class FormatReinterpreterBase {
public:
    FormatReinterpreterBase() {
        read_fbo.Create();
        draw_fbo.Create();
    }

    virtual ~FormatReinterpreterBase() = default;

    virtual PixelFormat GetSourceFormat() const = 0;
    virtual void Reinterpret(const OGLTexture& src_tex, Common::Rectangle<u32> src_rect,
                             const OGLTexture& dst_tex, Common::Rectangle<u32> dst_rect) = 0;

protected:
    OGLFramebuffer read_fbo, draw_fbo;
};

using ReinterpreterList = std::vector<std::unique_ptr<FormatReinterpreterBase>>;

class FormatReinterpreterOpenGL : NonCopyable {
public:
    FormatReinterpreterOpenGL();
    ~FormatReinterpreterOpenGL() = default;

    const ReinterpreterList& GetPossibleReinterpretations(PixelFormat dst_format);

private:
    std::array<ReinterpreterList, PIXEL_FORMAT_COUNT> reinterpreters;
};

} // namespace OpenGL
