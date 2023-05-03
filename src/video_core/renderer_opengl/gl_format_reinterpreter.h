// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/math_util.h"
#include "video_core/rasterizer_cache/pixel_format.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"

namespace OpenGL {

class Surface;

class FormatReinterpreterBase {
public:
    FormatReinterpreterBase() {
        read_fbo.Create();
        draw_fbo.Create();
    }

    virtual ~FormatReinterpreterBase() = default;

    virtual VideoCore::PixelFormat GetSourceFormat() const = 0;
    virtual void Reinterpret(Surface& source, Common::Rectangle<u32> src_rect, Surface& dest,
                             Common::Rectangle<u32> dst_rect) = 0;

protected:
    OGLFramebuffer read_fbo;
    OGLFramebuffer draw_fbo;
};

using ReinterpreterList = std::vector<std::unique_ptr<FormatReinterpreterBase>>;

class RGBA4toRGB5A1 final : public FormatReinterpreterBase {
public:
    RGBA4toRGB5A1();

    VideoCore::PixelFormat GetSourceFormat() const override {
        return VideoCore::PixelFormat::RGBA4;
    }

    void Reinterpret(Surface& source, Common::Rectangle<u32> src_rect, Surface& dest,
                     Common::Rectangle<u32> dst_rect) override;

private:
    OGLProgram program;
    GLint dst_size_loc{-1};
    GLint src_size_loc{-1};
    GLint src_offset_loc{-1};
    OGLVertexArray vao;
};

class ShaderD24S8toRGBA8 final : public FormatReinterpreterBase {
public:
    ShaderD24S8toRGBA8();

    VideoCore::PixelFormat GetSourceFormat() const override {
        return VideoCore::PixelFormat::D24S8;
    }

    void Reinterpret(Surface& source, Common::Rectangle<u32> src_rect, Surface& dest,
                     Common::Rectangle<u32> dst_rect) override;

private:
    bool use_texture_view{};
    OGLProgram program{};
    GLint dst_size_loc{-1};
    GLint src_size_loc{-1};
    GLint src_offset_loc{-1};
    OGLVertexArray vao{};
    OGLTexture temp_tex{};
    Common::Rectangle<u32> temp_rect{0, 0, 0, 0};
};

} // namespace OpenGL
