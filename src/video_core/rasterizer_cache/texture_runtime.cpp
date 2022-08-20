// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/scope_exit.h"
#include "video_core/rasterizer_cache/rasterizer_cache_utils.h"
#include "video_core/rasterizer_cache/texture_runtime.h"
#include "video_core/renderer_opengl/gl_state.h"

namespace OpenGL {

GLbitfield ToBufferMask(Aspect aspect) {
    switch (aspect) {
    case Aspect::Color:
        return GL_COLOR_BUFFER_BIT;
    case Aspect::Depth:
        return GL_DEPTH_BUFFER_BIT;
    case Aspect::DepthStencil:
        return GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    }
}

TextureRuntime::TextureRuntime() {
    read_fbo.Create();
    draw_fbo.Create();
}

void TextureRuntime::ReadTexture(const OGLTexture& tex, Subresource subresource,
                                 const FormatTuple& tuple, u8* pixels) {

    OpenGLState prev_state = OpenGLState::GetCurState();
    SCOPE_EXIT({ prev_state.Apply(); });

    OpenGLState state;
    state.ResetTexture(tex.handle);
    state.draw.read_framebuffer = read_fbo.handle;
    state.Apply();

    const u32 level = subresource.level;
    switch (subresource.aspect) {
    case Aspect::Color:
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               tex.handle, level);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               0, 0);
        break;
    case Aspect::Depth:
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                               tex.handle, level);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        break;
    case Aspect::DepthStencil:
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               tex.handle, level);
        break;
    }

    const auto& rect = subresource.region;
    glReadPixels(rect.left, rect.bottom, rect.GetWidth(), rect.GetHeight(),
                 tuple.format, tuple.type, pixels);
}

bool TextureRuntime::ClearTexture(const OGLTexture& tex, Subresource subresource,
                                  ClearValue value) {
    OpenGLState prev_state = OpenGLState::GetCurState();
    SCOPE_EXIT({ prev_state.Apply(); });

    // Setup scissor rectangle according to the clear rectangle
    const auto& clear_rect = subresource.region;
    OpenGLState state;
    state.scissor.enabled = true;
    state.scissor.x = clear_rect.left;
    state.scissor.y = clear_rect.bottom;
    state.scissor.width = clear_rect.GetWidth();
    state.scissor.height = clear_rect.GetHeight();
    state.draw.draw_framebuffer = draw_fbo.handle;
    state.Apply();

    const u32 level = subresource.level;
    switch (subresource.aspect) {
    case Aspect::Color:
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               tex.handle, level);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               0, 0);

        state.color_mask.red_enabled = true;
        state.color_mask.green_enabled = true;
        state.color_mask.blue_enabled = true;
        state.color_mask.alpha_enabled = true;
        state.Apply();

        glClearBufferfv(GL_COLOR, 0, value.color.AsArray());
        break;
    case Aspect::Depth:
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                               tex.handle, level);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

        state.depth.write_mask = GL_TRUE;
        state.Apply();

        glClearBufferfv(GL_DEPTH, 0, &value.depth);
        break;
    case Aspect::DepthStencil:
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               tex.handle, level);

        state.depth.write_mask = GL_TRUE;
        state.stencil.write_mask = -1;
        state.Apply();

        glClearBufferfi(GL_DEPTH_STENCIL, 0, value.depth, value.stencil);
        break;
    }

    return true;
}

bool TextureRuntime::CopyTextures(const OGLTexture& src_tex, Subresource src_subresource,
                                  const OGLTexture& dst_tex, Subresource dst_subresource) {
    return true;
}

bool TextureRuntime::BlitTextures(const OGLTexture& src_tex, Subresource src_subresource,
                                  const OGLTexture& dst_tex, Subresource dst_subresource) {
    OpenGLState prev_state = OpenGLState::GetCurState();
    SCOPE_EXIT({ prev_state.Apply(); });

    OpenGLState state;
    state.draw.read_framebuffer = read_fbo.handle;
    state.draw.draw_framebuffer = draw_fbo.handle;
    state.Apply();

    auto BindAttachment = [src_level = src_subresource.level,
                           dst_level = dst_subresource.level](GLenum target, u32 src_tex,
                                                              u32 dst_tex) -> void {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, target, GL_TEXTURE_2D, src_tex, src_level);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, target, GL_TEXTURE_2D, dst_tex, dst_level);
    };

    // Sanity check; Can't blit a color texture to a depth buffer
    ASSERT(src_subresource.aspect == dst_subresource.aspect);
    switch (src_subresource.aspect) {
    case Aspect::Color:
        // Bind only color
        BindAttachment(GL_COLOR_ATTACHMENT0, src_tex.handle, dst_tex.handle);
        BindAttachment(GL_DEPTH_STENCIL_ATTACHMENT, 0, 0);
        break;
    case Aspect::Depth:
        // Bind only depth
        BindAttachment(GL_COLOR_ATTACHMENT0, 0, 0);
        BindAttachment(GL_DEPTH_ATTACHMENT, src_tex.handle, dst_tex.handle);
        BindAttachment(GL_STENCIL_ATTACHMENT, 0, 0);
        break;
    case Aspect::DepthStencil:
        // Bind to combined depth + stencil
        BindAttachment(GL_COLOR_ATTACHMENT0, 0, 0);
        BindAttachment(GL_DEPTH_STENCIL_ATTACHMENT, src_tex.handle, dst_tex.handle);
        break;
    }

    // TODO (wwylele): use GL_NEAREST for shadow map texture
    // Note: shadow map is treated as RGBA8 format in PICA, as well as in the rasterizer cache, but
    // doing linear intepolation componentwise would cause incorrect value. However, for a
    // well-programmed game this code path should be rarely executed for shadow map with
    // inconsistent scale.
    const GLenum filter = src_subresource.aspect == Aspect::Color ? GL_LINEAR : GL_NEAREST;
    const auto& src_rect = src_subresource.region;
    const auto& dst_rect = dst_subresource.region;
    glBlitFramebuffer(src_rect.left, src_rect.bottom, src_rect.right, src_rect.top,
                      dst_rect.left, dst_rect.bottom, dst_rect.right, dst_rect.top,
                      ToBufferMask(src_subresource.aspect), filter);

    return true;
}

void TextureRuntime::GenerateMipmaps(const OGLTexture& tex, u32 max_level) {
    OpenGLState prev_state = OpenGLState::GetCurState();
    SCOPE_EXIT({ prev_state.Apply(); });

    OpenGLState state;
    state.texture_units[0].texture_2d = tex.handle;
    state.Apply();

    glActiveTexture(GL_TEXTURE0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, max_level);

    glGenerateMipmap(GL_TEXTURE_2D);
}

} // namespace OpenGL
