// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/settings.h"
#include "video_core/rasterizer_cache/pixel_format.h"
#include "video_core/renderer_opengl/gl_blit_helper.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_texture_runtime.h"

#include "video_core/host_shaders/full_screen_triangle_vert.h"
#include "video_core/host_shaders/texture_filtering/bicubic_frag.h"
#include "video_core/host_shaders/texture_filtering/nearest_neighbor_frag.h"
#include "video_core/host_shaders/texture_filtering/refine_frag.h"
#include "video_core/host_shaders/texture_filtering/scale_force_frag.h"
#include "video_core/host_shaders/texture_filtering/x_gradient_frag.h"
#include "video_core/host_shaders/texture_filtering/xbrz_freescale_frag.h"
#include "video_core/host_shaders/texture_filtering/y_gradient_frag.h"

namespace OpenGL {

using Settings::TextureFilter;
using VideoCore::SurfaceType;

namespace {

struct TempTexture {
    OGLTexture tex;
    OGLFramebuffer fbo;
};

OGLSampler CreateSampler(GLenum filter) {
    OGLSampler sampler;
    sampler.Create();
    glSamplerParameteri(sampler.handle, GL_TEXTURE_MIN_FILTER, filter);
    glSamplerParameteri(sampler.handle, GL_TEXTURE_MAG_FILTER, filter);
    glSamplerParameteri(sampler.handle, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glSamplerParameteri(sampler.handle, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    return sampler;
}

OGLProgram CreateProgram(std::string_view frag) {
    OGLProgram program;
    program.Create(HostShaders::FULL_SCREEN_TRIANGLE_VERT, frag);
    glProgramUniform2f(program.handle, 0, 1.f, 1.f);
    glProgramUniform2f(program.handle, 1, 0.f, 0.f);
    return program;
}

} // Anonymous namespace

BlitHelper::BlitHelper(TextureRuntime& runtime_)
    : runtime{runtime_}, linear_sampler{CreateSampler(GL_LINEAR)},
      nearest_sampler{CreateSampler(GL_NEAREST)}, bicubic_program{CreateProgram(
                                                      HostShaders::BICUBIC_FRAG)},
      nearest_program{CreateProgram(HostShaders::NEAREST_NEIGHBOR_FRAG)},
      scale_force_program{CreateProgram(HostShaders::SCALE_FORCE_FRAG)},
      xbrz_program{CreateProgram(HostShaders::XBRZ_FREESCALE_FRAG)},
      gradient_x_program{CreateProgram(HostShaders::X_GRADIENT_FRAG)},
      gradient_y_program{CreateProgram(HostShaders::Y_GRADIENT_FRAG)},
      refine_program{CreateProgram(HostShaders::REFINE_FRAG)} {
    vao.Create();
    filter_fbo.Create();
    state.draw.vertex_array = vao.handle;
    for (u32 i = 0; i < 3; i++) {
        state.texture_units[i].sampler = i == 2 ? nearest_sampler.handle : linear_sampler.handle;
    }
}

BlitHelper::~BlitHelper() = default;

bool BlitHelper::Filter(Surface& surface, const VideoCore::TextureBlit& blit) {
    // Filtering to depth stencil surfaces isn't supported.
    if (surface.type == SurfaceType::Depth || surface.type == SurfaceType::DepthStencil) {
        return false;
    }
    // Avoid filtering for mipmaps as the result often looks terrible.
    if (blit.src_level != 0) {
        return true;
    }

    const OpenGLState prev_state = OpenGLState::GetCurState();
    state.texture_units[0].texture_2d = surface.Handle(0);

    const auto filter{Settings::values.texture_filter.GetValue()};
    switch (filter) {
    case TextureFilter::None:
        break;
    case TextureFilter::Anime4K:
        FilterAnime4K(surface, blit);
        break;
    case TextureFilter::Bicubic:
        FilterBicubic(surface, blit);
        break;
    case TextureFilter::NearestNeighbor:
        FilterNearest(surface, blit);
        break;
    case TextureFilter::ScaleForce:
        FilterScaleForce(surface, blit);
        break;
    case TextureFilter::xBRZ:
        FilterXbrz(surface, blit);
        break;
    }

    prev_state.Apply();
    return true;
}

void BlitHelper::FilterAnime4K(Surface& surface, const VideoCore::TextureBlit& blit) {
    static constexpr u8 internal_scale_factor = 2;

    const auto& tuple = surface.Tuple();
    const u32 src_width = blit.src_rect.GetWidth();
    const u32 src_height = blit.src_rect.GetHeight();
    const auto temp_rect{blit.src_rect * internal_scale_factor};

    const auto setup_temp_tex = [&](GLint internal_format, GLint format, u32 width, u32 height) {
        TempTexture texture;
        texture.fbo.Create();
        texture.tex.Create();
        state.texture_units[1].texture_2d = texture.tex.handle;
        state.draw.draw_framebuffer = texture.fbo.handle;
        state.Apply();
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, texture.tex.handle);
        glTexStorage2D(GL_TEXTURE_2D, 1, internal_format, width, height);
        return texture;
    };

    // Create intermediate textures
    auto SRC = setup_temp_tex(tuple.internal_format, tuple.format, src_width, src_height);
    auto XY = setup_temp_tex(GL_RG16F, GL_RG, temp_rect.GetWidth(), temp_rect.GetHeight());
    auto LUMAD = setup_temp_tex(GL_R16F, GL_RED, temp_rect.GetWidth(), temp_rect.GetHeight());

    // Copy to SRC
    glCopyImageSubData(surface.Handle(0), GL_TEXTURE_2D, 0, blit.src_rect.left,
                       blit.src_rect.bottom, 0, SRC.tex.handle, GL_TEXTURE_2D, 0, 0, 0, 0,
                       src_width, src_height, 1);

    state.texture_units[0].texture_2d = SRC.tex.handle;
    state.texture_units[1].texture_2d = LUMAD.tex.handle;
    state.texture_units[2].texture_2d = XY.tex.handle;

    // gradient x pass
    Draw(gradient_x_program, XY.tex.handle, XY.fbo.handle, 0, temp_rect);

    // gradient y pass
    Draw(gradient_y_program, LUMAD.tex.handle, LUMAD.fbo.handle, 0, temp_rect);

    // refine pass
    Draw(refine_program, surface.Handle(), filter_fbo.handle, blit.dst_level, blit.dst_rect);

    // These will have handles from the previous texture that was filtered, reset them to avoid
    // binding invalid textures.
    state.texture_units[0].texture_2d = 0;
    state.texture_units[1].texture_2d = 0;
    state.texture_units[2].texture_2d = 0;
    state.Apply();
}

void BlitHelper::FilterBicubic(Surface& surface, const VideoCore::TextureBlit& blit) {
    SetParams(bicubic_program, surface.Extent(), blit.src_rect);
    Draw(bicubic_program, surface.Handle(), filter_fbo.handle, blit.dst_level, blit.dst_rect);
}

void BlitHelper::FilterNearest(Surface& surface, const VideoCore::TextureBlit& blit) {
    state.texture_units[2].texture_2d = surface.Handle(0);
    SetParams(nearest_program, surface.Extent(), blit.src_rect);
    Draw(nearest_program, surface.Handle(), filter_fbo.handle, blit.dst_level, blit.dst_rect);
}

void BlitHelper::FilterScaleForce(Surface& surface, const VideoCore::TextureBlit& blit) {
    SetParams(scale_force_program, surface.Extent(), blit.src_rect);
    Draw(scale_force_program, surface.Handle(), filter_fbo.handle, blit.dst_level, blit.dst_rect);
}

void BlitHelper::FilterXbrz(Surface& surface, const VideoCore::TextureBlit& blit) {
    glProgramUniform1f(xbrz_program.handle, 2, static_cast<GLfloat>(surface.res_scale));
    SetParams(xbrz_program, surface.Extent(), blit.src_rect);
    Draw(xbrz_program, surface.Handle(), filter_fbo.handle, blit.dst_level, blit.dst_rect);
}

void BlitHelper::SetParams(OGLProgram& program, const VideoCore::Extent& src_extent,
                           Common::Rectangle<u32> src_rect) {
    glProgramUniform2f(
        program.handle, 0,
        static_cast<float>(src_rect.right - src_rect.left) / static_cast<float>(src_extent.width),
        static_cast<float>(src_rect.top - src_rect.bottom) / static_cast<float>(src_extent.height));
    glProgramUniform2f(program.handle, 1,
                       static_cast<float>(src_rect.left) / static_cast<float>(src_extent.width),
                       static_cast<float>(src_rect.bottom) / static_cast<float>(src_extent.height));
}

void BlitHelper::Draw(OGLProgram& program, GLuint dst_tex, GLuint dst_fbo, u32 dst_level,
                      Common::Rectangle<u32> dst_rect) {
    state.draw.draw_framebuffer = dst_fbo;
    state.draw.shader_program = program.handle;
    state.viewport.x = dst_rect.left;
    state.viewport.y = dst_rect.bottom;
    state.viewport.width = dst_rect.GetWidth();
    state.viewport.height = dst_rect.GetHeight();
    state.Apply();

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_tex,
                           dst_level);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
}

} // namespace OpenGL
