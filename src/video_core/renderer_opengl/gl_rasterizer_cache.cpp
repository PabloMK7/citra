// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <atomic>
#include <cstring>
#include <iterator>
#include <unordered_set>
#include <utility>
#include <vector>
#include <glad/glad.h>
#include "common/bit_field.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "common/vector_math.h"
#include "core/frontend/emu_window.h"
#include "core/memory.h"
#include "core/settings.h"
#include "video_core/pica_state.h"
#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/texture/texture_decode.h"
#include "video_core/utils.h"
#include "video_core/video_core.h"

struct FormatTuple {
    GLint internal_format;
    GLenum format;
    GLenum type;
};

static const std::array<FormatTuple, 5> fb_format_tuples = {{
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8},     // RGBA8
    {GL_RGB8, GL_BGR, GL_UNSIGNED_BYTE},              // RGB8
    {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1}, // RGB5A1
    {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},     // RGB565
    {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},   // RGBA4
}};

static const std::array<FormatTuple, 4> depth_format_tuples = {{
    {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT}, // D16
    {},
    {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT},   // D24
    {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8}, // D24S8
}};

static bool FillSurface(const Surface& surface, const u8* fill_data,
                        const MathUtil::Rectangle<u32>& fill_rect) {
    OpenGLState state = OpenGLState::GetCurState();

    OpenGLState prev_state = state;
    SCOPE_EXIT({ prev_state.Apply(); });

    OpenGLState::ResetTexture(surface->texture.handle);

    state.scissor.enabled = true;
    state.scissor.x = static_cast<GLint>(fill_rect.left);
    state.scissor.y = static_cast<GLint>(std::min(fill_rect.top, fill_rect.bottom));
    state.scissor.width = static_cast<GLsizei>(fill_rect.GetWidth());
    state.scissor.height = static_cast<GLsizei>(fill_rect.GetHeight());

    state.draw.draw_framebuffer = transfer_framebuffers[1].handle;
    state.Apply();

    if (surface->type == SurfaceType::Color || surface->type == SurfaceType::Texture) {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                               surface->texture.handle, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);

        Pica::Texture::TextureInfo tex_info{};
        tex_info.format = static_cast<Pica::TexturingRegs::TextureFormat>(surface->pixel_format);
        Math::Vec4<u8> color = Pica::Texture::LookupTexture(fill_data, 0, 0, tex_info);

        std::array<GLfloat, 4> color_values = {color.x / 255.f, color.y / 255.f, color.z / 255.f,
                                               color.w / 255.f};

        state.color_mask.red_enabled = GL_TRUE;
        state.color_mask.green_enabled = GL_TRUE;
        state.color_mask.blue_enabled = GL_TRUE;
        state.color_mask.alpha_enabled = GL_TRUE;
        state.Apply();
        glClearBufferfv(GL_COLOR, 0, &color_values[0]);
    } else if (surface->type == SurfaceType::Depth) {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                               surface->texture.handle, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

        u32 value_32bit = 0;
        GLfloat value_float;

        if (surface->pixel_format == SurfaceParams::PixelFormat::D16) {
            std::memcpy(&value_32bit, fill_data, 2);
            value_float = value_32bit / 65535.0f; // 2^16 - 1
        } else if (surface->pixel_format == SurfaceParams::PixelFormat::D24) {
            std::memcpy(&value_32bit, fill_data, 3);
            value_float = value_32bit / 16777215.0f; // 2^24 - 1
        }

        state.depth.write_mask = GL_TRUE;
        state.Apply();
        glClearBufferfv(GL_DEPTH, 0, &value_float);
    } else if (surface->type == SurfaceType::DepthStencil) {
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               surface->texture.handle, 0);

        u32 value_32bit;
        std::memcpy(&value_32bit, fill_data, 4);

        GLfloat value_float = (value_32bit & 0xFFFFFF) / 16777215.0f; // 2^24 - 1
        GLint value_int = (value_32bit >> 24);

        state.depth.write_mask = GL_TRUE;
        state.stencil.write_mask = -1;
        state.Apply();
        glClearBufferfi(GL_DEPTH_STENCIL, 0, value_float, value_int);
    }
    return true;
}

RasterizerCacheOpenGL::RasterizerCacheOpenGL() {
    transfer_framebuffers[0].Create();
    transfer_framebuffers[1].Create();
}

RasterizerCacheOpenGL::~RasterizerCacheOpenGL() {
    FlushAll();
}

template <bool morton_to_gl, PixelFormat format>
static void MortonCopyTile(u32 stride, u8* tile_buffer, u8* gl_buffer) {
    constexpr u32 bytes_per_pixel = SurfaceParams::GetFormatBpp(format) / 8;
    constexpr u32 gl_bytes_per_pixel = CachedSurface::GetGLBytesPerPixel(format);
    for (u32 y = 0; y < 8; ++y) {
        for (u32 x = 0; x < 8; ++x) {
            u8* tile_ptr = tile_buffer + VideoCore::MortonInterleave(x, y) * bytes_per_pixel;
            u8* gl_ptr = gl_buffer + ((7 - y) * stride + x) * gl_bytes_per_pixel;
            if (morton_to_gl) {
                if (format == PixelFormat::D24S8) {
                    gl_ptr[0] = tile_ptr[3];
                    std::memcpy(gl_ptr + 1, tile_ptr, 3);
                } else {
                    std::memcpy(gl_ptr, tile_ptr, bytes_per_pixel);
                }
            } else {
                if (format == PixelFormat::D24S8) {
                    std::memcpy(tile_ptr, gl_ptr + 1, 3);
                    tile_ptr[3] = gl_ptr[0];
                } else {
                    std::memcpy(tile_ptr, gl_ptr, bytes_per_pixel);
                }
            }
        }
    }
}

template <bool morton_to_gl, PixelFormat format>
static void MortonCopy(u32 stride, u32 height, u8* gl_buffer, PAddr base, PAddr start, PAddr end) {
    constexpr u32 bytes_per_pixel = SurfaceParams::GetFormatBpp(format) / 8;
    constexpr u32 tile_size = bytes_per_pixel * 64;

    constexpr u32 gl_bytes_per_pixel = CachedSurface::GetGLBytesPerPixel(format);
    static_assert(gl_bytes_per_pixel >= bytes_per_pixel, "");
    gl_buffer += gl_bytes_per_pixel - bytes_per_pixel;

    const PAddr aligned_down_start = base + Common::AlignDown(start - base, tile_size);
    const PAddr aligned_start = base + Common::AlignUp(start - base, tile_size);
    const PAddr aligned_end = base + Common::AlignDown(end - base, tile_size);

    ASSERT(!morton_to_gl || (aligned_start == start && aligned_end == end));

    const u32 begin_pixel_index = (aligned_down_start - base) / bytes_per_pixel;
    u32 x = (begin_pixel_index % (stride * 8)) / 8;
    u32 y = (begin_pixel_index / (stride * 8)) * 8;

    gl_buffer += ((height - 8 - y) * stride + x) * gl_bytes_per_pixel;

    auto glbuf_next_tile = [&] {
        x = (x + 8) % stride;
        gl_buffer += 8 * gl_bytes_per_pixel;
        if (!x) {
            y += 8;
            gl_buffer -= stride * 9 * gl_bytes_per_pixel;
        }
    };

    u8* tile_buffer = Memory::GetPhysicalPointer(start);

    if (start < aligned_start && !morton_to_gl) {
        std::array<u8, tile_size> tmp_buf;
        MortonCopyTile<morton_to_gl, format>(stride, &tmp_buf[0], gl_buffer);
        std::memcpy(tile_buffer, &tmp_buf[start - aligned_down_start],
                    std::min(aligned_start, end) - start);

        tile_buffer += aligned_start - start;
        glbuf_next_tile();
    }

    u8* const buffer_end = tile_buffer + aligned_end - aligned_start;
    while (tile_buffer < buffer_end) {
        MortonCopyTile<morton_to_gl, format>(stride, tile_buffer, gl_buffer);
        tile_buffer += tile_size;
        glbuf_next_tile();
    }

    if (end > std::max(aligned_start, aligned_end) && !morton_to_gl) {
        std::array<u8, tile_size> tmp_buf;
        MortonCopyTile<morton_to_gl, format>(stride, &tmp_buf[0], gl_buffer);
        std::memcpy(tile_buffer, &tmp_buf[0], end - aligned_end);
    }
}

static constexpr std::array<void (*)(u32, u32, u8*, PAddr, PAddr, PAddr), 18> morton_to_gl_fns = {
    MortonCopy<true, PixelFormat::RGBA8>,  // 0
    MortonCopy<true, PixelFormat::RGB8>,   // 1
    MortonCopy<true, PixelFormat::RGB5A1>, // 2
    MortonCopy<true, PixelFormat::RGB565>, // 3
    MortonCopy<true, PixelFormat::RGBA4>,  // 4
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,                             // 5 - 13
    MortonCopy<true, PixelFormat::D16>,  // 14
    nullptr,                             // 15
    MortonCopy<true, PixelFormat::D24>,  // 16
    MortonCopy<true, PixelFormat::D24S8> // 17
};

static constexpr std::array<void (*)(u32, u32, u8*, PAddr, PAddr, PAddr), 18> gl_to_morton_fns = {
    MortonCopy<false, PixelFormat::RGBA8>,  // 0
    MortonCopy<false, PixelFormat::RGB8>,   // 1
    MortonCopy<false, PixelFormat::RGB5A1>, // 2
    MortonCopy<false, PixelFormat::RGB565>, // 3
    MortonCopy<false, PixelFormat::RGBA4>,  // 4
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,
    nullptr,                              // 5 - 13
    MortonCopy<false, PixelFormat::D16>,  // 14
    nullptr,                              // 15
    MortonCopy<false, PixelFormat::D24>,  // 16
    MortonCopy<false, PixelFormat::D24S8> // 17
};

void RasterizerCacheOpenGL::BlitTextures(GLuint src_tex, GLuint dst_tex,
                                         CachedSurface::SurfaceType type,
                                         const MathUtil::Rectangle<int>& src_rect,
                                         const MathUtil::Rectangle<int>& dst_rect) {
    using SurfaceType = CachedSurface::SurfaceType;

    OpenGLState cur_state = OpenGLState::GetCurState();

    // Make sure textures aren't bound to texture units, since going to bind them to framebuffer
    // components
    OpenGLState::ResetTexture(src_tex);
    OpenGLState::ResetTexture(dst_tex);

    // Keep track of previous framebuffer bindings
    GLuint old_fbs[2] = {cur_state.draw.read_framebuffer, cur_state.draw.draw_framebuffer};
    cur_state.draw.read_framebuffer = transfer_framebuffers[0].handle;
    cur_state.draw.draw_framebuffer = transfer_framebuffers[1].handle;
    cur_state.Apply();

    u32 buffers = 0;

    if (type == SurfaceType::Color || type == SurfaceType::Texture) {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, src_tex,
                               0);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_tex,
                               0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0,
                               0);

        buffers = GL_COLOR_BUFFER_BIT;
    } else if (type == SurfaceType::Depth) {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, src_tex, 0);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, dst_tex, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);

        buffers = GL_DEPTH_BUFFER_BIT;
    } else if (type == SurfaceType::DepthStencil) {
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               src_tex, 0);

        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
        glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                               dst_tex, 0);

        buffers = GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT;
    }

    glBlitFramebuffer(src_rect.left, src_rect.top, src_rect.right, src_rect.bottom, dst_rect.left,
                      dst_rect.top, dst_rect.right, dst_rect.bottom, buffers,
                      buffers == GL_COLOR_BUFFER_BIT ? GL_LINEAR : GL_NEAREST);

    // Restore previous framebuffer bindings
    cur_state.draw.read_framebuffer = old_fbs[0];
    cur_state.draw.draw_framebuffer = old_fbs[1];
    cur_state.Apply();
}

bool RasterizerCacheOpenGL::TryBlitSurfaces(CachedSurface* src_surface,
                                            const MathUtil::Rectangle<int>& src_rect,
                                            CachedSurface* dst_surface,
                                            const MathUtil::Rectangle<int>& dst_rect) {

    if (!CachedSurface::CheckFormatsBlittable(src_surface->pixel_format,
                                              dst_surface->pixel_format)) {
        return false;
    }

    BlitTextures(src_surface->texture.handle, dst_surface->texture.handle,
                 CachedSurface::GetFormatType(src_surface->pixel_format), src_rect, dst_rect);
    return true;
}

static void AllocateSurfaceTexture(GLuint texture, CachedSurface::PixelFormat pixel_format,
                                   u32 width, u32 height) {
    // Allocate an uninitialized texture of appropriate size and format for the surface
    using SurfaceType = CachedSurface::SurfaceType;

    OpenGLState cur_state = OpenGLState::GetCurState();

    // Keep track of previous texture bindings
    GLuint old_tex = cur_state.texture_units[0].texture_2d;
    cur_state.texture_units[0].texture_2d = texture;
    cur_state.Apply();
    glActiveTexture(GL_TEXTURE0);

    SurfaceType type = CachedSurface::GetFormatType(pixel_format);

    FormatTuple tuple;
    if (type == SurfaceType::Color) {
        ASSERT((size_t)pixel_format < fb_format_tuples.size());
        tuple = fb_format_tuples[(unsigned int)pixel_format];
    } else if (type == SurfaceType::Depth || type == SurfaceType::DepthStencil) {
        size_t tuple_idx = (size_t)pixel_format - 14;
        ASSERT(tuple_idx < depth_format_tuples.size());
        tuple = depth_format_tuples[tuple_idx];
    } else {
        tuple = {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
    }

    glTexImage2D(GL_TEXTURE_2D, 0, tuple.internal_format, width, height, 0, tuple.format,
                 tuple.type, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Restore previous texture bindings
    cur_state.texture_units[0].texture_2d = old_tex;
    cur_state.Apply();
}

MICROPROFILE_DEFINE(OpenGL_SurfaceUpload, "OpenGL", "Surface Upload", MP_RGB(128, 64, 192));
CachedSurface* RasterizerCacheOpenGL::GetSurface(const CachedSurface& params, bool match_res_scale,
                                                 bool load_if_create) {
    using PixelFormat = CachedSurface::PixelFormat;
    using SurfaceType = CachedSurface::SurfaceType;

    if (params.addr == 0) {
        return nullptr;
    }

    u32 params_size =
        params.width * params.height * CachedSurface::GetFormatBpp(params.pixel_format) / 8;

    // Check for an exact match in existing surfaces
    CachedSurface* best_exact_surface = nullptr;
    float exact_surface_goodness = -1.f;

    auto surface_interval =
        boost::icl::interval<PAddr>::right_open(params.addr, params.addr + params_size);
    auto range = surface_cache.equal_range(surface_interval);
    for (auto it = range.first; it != range.second; ++it) {
        for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            CachedSurface* surface = it2->get();

            // Check if the request matches the surface exactly
            if (params.addr == surface->addr && params.width == surface->width &&
                params.height == surface->height && params.pixel_format == surface->pixel_format) {
                // Make sure optional param-matching criteria are fulfilled
                bool tiling_match = (params.is_tiled == surface->is_tiled);
                bool res_scale_match = (params.res_scale_width == surface->res_scale_width &&
                                        params.res_scale_height == surface->res_scale_height);
                if (!match_res_scale || res_scale_match) {
                    // Prioritize same-tiling and highest resolution surfaces
                    float match_goodness =
                        (float)tiling_match + surface->res_scale_width * surface->res_scale_height;
                    if (match_goodness > exact_surface_goodness || surface->dirty) {
                        exact_surface_goodness = match_goodness;
                        best_exact_surface = surface;
                    }
                }
            }
        }
    }

    // Return the best exact surface if found
    if (best_exact_surface != nullptr) {
        return best_exact_surface;
    }

    // No matching surfaces found, so create a new one
    u8* texture_src_data = Memory::GetPhysicalPointer(params.addr);
    if (texture_src_data == nullptr) {
        return nullptr;
    }

    MICROPROFILE_SCOPE(OpenGL_SurfaceUpload);

    // Stride only applies to linear images.
    ASSERT(params.pixel_stride == 0 || !params.is_tiled);

    std::shared_ptr<CachedSurface> new_surface = std::make_shared<CachedSurface>();

    new_surface->addr = params.addr;
    new_surface->size = params_size;

    new_surface->texture.Create();
    new_surface->width = params.width;
    new_surface->height = params.height;
    new_surface->pixel_stride = params.pixel_stride;
    new_surface->res_scale_width = params.res_scale_width;
    new_surface->res_scale_height = params.res_scale_height;

    new_surface->is_tiled = params.is_tiled;
    new_surface->pixel_format = params.pixel_format;
    new_surface->dirty = false;

    if (!load_if_create) {
        // Don't load any data; just allocate the surface's texture
        AllocateSurfaceTexture(new_surface->texture.handle, new_surface->pixel_format,
                               new_surface->GetScaledWidth(), new_surface->GetScaledHeight());
    } else {
        // TODO: Consider attempting subrect match in existing surfaces and direct blit here instead
        // of memory upload below if that's a common scenario in some game

        Memory::RasterizerFlushRegion(params.addr, params_size);

        // Load data from memory to the new surface
        OpenGLState cur_state = OpenGLState::GetCurState();

        GLuint old_tex = cur_state.texture_units[0].texture_2d;
        cur_state.texture_units[0].texture_2d = new_surface->texture.handle;
        cur_state.Apply();
        glActiveTexture(GL_TEXTURE0);

        if (!new_surface->is_tiled) {
            // TODO: Ensure this will always be a color format, not a depth or other format
            ASSERT((size_t)new_surface->pixel_format < fb_format_tuples.size());
            const FormatTuple& tuple = fb_format_tuples[(unsigned int)params.pixel_format];

            glPixelStorei(GL_UNPACK_ROW_LENGTH, (GLint)new_surface->pixel_stride);
            glTexImage2D(GL_TEXTURE_2D, 0, tuple.internal_format, params.width, params.height, 0,
                         tuple.format, tuple.type, texture_src_data);
            glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        } else {
            SurfaceType type = CachedSurface::GetFormatType(new_surface->pixel_format);
            if (type != SurfaceType::Depth && type != SurfaceType::DepthStencil) {
                FormatTuple tuple;
                if ((size_t)params.pixel_format < fb_format_tuples.size()) {
                    tuple = fb_format_tuples[(unsigned int)params.pixel_format];
                } else {
                    // Texture
                    tuple = {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};
                }

                std::vector<Math::Vec4<u8>> tex_buffer(params.width * params.height);

                Pica::Texture::TextureInfo tex_info;
                tex_info.width = params.width;
                tex_info.height = params.height;
                tex_info.format = (Pica::TexturingRegs::TextureFormat)params.pixel_format;
                tex_info.SetDefaultStride();
                tex_info.physical_address = params.addr;

                for (unsigned y = 0; y < params.height; ++y) {
                    for (unsigned x = 0; x < params.width; ++x) {
                        tex_buffer[x + params.width * y] = Pica::Texture::LookupTexture(
                            texture_src_data, x, params.height - 1 - y, tex_info);
                    }
                }

                glTexImage2D(GL_TEXTURE_2D, 0, tuple.internal_format, params.width, params.height,
                             0, GL_RGBA, GL_UNSIGNED_BYTE, tex_buffer.data());
            } else {
                // Depth/Stencil formats need special treatment since they aren't sampleable using
                // LookupTexture and can't use RGBA format
                size_t tuple_idx = (size_t)params.pixel_format - 14;
                ASSERT(tuple_idx < depth_format_tuples.size());
                const FormatTuple& tuple = depth_format_tuples[tuple_idx];

                u32 bytes_per_pixel = CachedSurface::GetFormatBpp(params.pixel_format) / 8;

                // OpenGL needs 4 bpp alignment for D24 since using GL_UNSIGNED_INT as type
                bool use_4bpp = (params.pixel_format == PixelFormat::D24);

                u32 gl_bytes_per_pixel = use_4bpp ? 4 : bytes_per_pixel;

                std::vector<u8> temp_fb_depth_buffer(params.width * params.height *
                                                     gl_bytes_per_pixel);

                u8* temp_fb_depth_buffer_ptr =
                    use_4bpp ? temp_fb_depth_buffer.data() + 1 : temp_fb_depth_buffer.data();

                MortonCopyPixels(params.pixel_format, params.width, params.height, bytes_per_pixel,
                                 gl_bytes_per_pixel, texture_src_data, temp_fb_depth_buffer_ptr,
                                 true);

                glTexImage2D(GL_TEXTURE_2D, 0, tuple.internal_format, params.width, params.height,
                             0, tuple.format, tuple.type, temp_fb_depth_buffer.data());
            }
        }

        // If not 1x scale, blit 1x texture to a new scaled texture and replace texture in surface
        if (new_surface->res_scale_width != 1.f || new_surface->res_scale_height != 1.f) {
            OGLTexture scaled_texture;
            scaled_texture.Create();

            AllocateSurfaceTexture(scaled_texture.handle, new_surface->pixel_format,
                                   new_surface->GetScaledWidth(), new_surface->GetScaledHeight());
            BlitTextures(new_surface->texture.handle, scaled_texture.handle,
                         CachedSurface::GetFormatType(new_surface->pixel_format),
                         MathUtil::Rectangle<int>(0, 0, new_surface->width, new_surface->height),
                         MathUtil::Rectangle<int>(0, 0, new_surface->GetScaledWidth(),
                                                  new_surface->GetScaledHeight()));

            new_surface->texture.Release();
            new_surface->texture.handle = scaled_texture.handle;
            scaled_texture.handle = 0;
            cur_state.texture_units[0].texture_2d = new_surface->texture.handle;
            cur_state.Apply();
        }

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

        cur_state.texture_units[0].texture_2d = old_tex;
        cur_state.Apply();
    }

    Memory::RasterizerMarkRegionCached(new_surface->addr, new_surface->size, 1);
    surface_cache.add(std::make_pair(boost::icl::interval<PAddr>::right_open(
                                         new_surface->addr, new_surface->addr + new_surface->size),
                                     std::set<std::shared_ptr<CachedSurface>>({new_surface})));
    return new_surface.get();
}

CachedSurface* RasterizerCacheOpenGL::GetSurfaceRect(const CachedSurface& params,
                                                     bool match_res_scale, bool load_if_create,
                                                     MathUtil::Rectangle<int>& out_rect) {
    if (params.addr == 0) {
        return nullptr;
    }

    u32 total_pixels = params.width * params.height;
    u32 params_size = total_pixels * CachedSurface::GetFormatBpp(params.pixel_format) / 8;

    // Attempt to find encompassing surfaces
    CachedSurface* best_subrect_surface = nullptr;
    float subrect_surface_goodness = -1.f;

    auto surface_interval =
        boost::icl::interval<PAddr>::right_open(params.addr, params.addr + params_size);
    auto cache_upper_bound = surface_cache.upper_bound(surface_interval);
    for (auto it = surface_cache.lower_bound(surface_interval); it != cache_upper_bound; ++it) {
        for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            CachedSurface* surface = it2->get();

            // Check if the request is contained in the surface
            if (params.addr >= surface->addr &&
                params.addr + params_size - 1 <= surface->addr + surface->size - 1 &&
                params.pixel_format == surface->pixel_format) {
                // Make sure optional param-matching criteria are fulfilled
                bool tiling_match = (params.is_tiled == surface->is_tiled);
                bool res_scale_match = (params.res_scale_width == surface->res_scale_width &&
                                        params.res_scale_height == surface->res_scale_height);
                if (!match_res_scale || res_scale_match) {
                    // Prioritize same-tiling and highest resolution surfaces
                    float match_goodness =
                        (float)tiling_match + surface->res_scale_width * surface->res_scale_height;
                    if (match_goodness > subrect_surface_goodness || surface->dirty) {
                        subrect_surface_goodness = match_goodness;
                        best_subrect_surface = surface;
                    }
                }
            }
        }
    }

    // Return the best subrect surface if found
    if (best_subrect_surface != nullptr) {
        unsigned int bytes_per_pixel =
            (CachedSurface::GetFormatBpp(best_subrect_surface->pixel_format) / 8);

        int x0, y0;

        if (!params.is_tiled) {
            u32 begin_pixel_index = (params.addr - best_subrect_surface->addr) / bytes_per_pixel;
            x0 = begin_pixel_index % best_subrect_surface->width;
            y0 = begin_pixel_index / best_subrect_surface->width;

            out_rect = MathUtil::Rectangle<int>(x0, y0, x0 + params.width, y0 + params.height);
        } else {
            u32 bytes_per_tile = 8 * 8 * bytes_per_pixel;
            u32 tiles_per_row = best_subrect_surface->width / 8;

            u32 begin_tile_index = (params.addr - best_subrect_surface->addr) / bytes_per_tile;
            x0 = begin_tile_index % tiles_per_row * 8;
            y0 = begin_tile_index / tiles_per_row * 8;

            // Tiled surfaces are flipped vertically in the rasterizer vs. 3DS memory.
            out_rect =
                MathUtil::Rectangle<int>(x0, best_subrect_surface->height - y0, x0 + params.width,
                                         best_subrect_surface->height - (y0 + params.height));
        }

        out_rect.left = (int)(out_rect.left * best_subrect_surface->res_scale_width);
        out_rect.right = (int)(out_rect.right * best_subrect_surface->res_scale_width);
        out_rect.top = (int)(out_rect.top * best_subrect_surface->res_scale_height);
        out_rect.bottom = (int)(out_rect.bottom * best_subrect_surface->res_scale_height);

        return best_subrect_surface;
    }

    // No subrect found - create and return a new surface
    if (!params.is_tiled) {
        out_rect = MathUtil::Rectangle<int>(0, 0, (int)(params.width * params.res_scale_width),
                                            (int)(params.height * params.res_scale_height));
    } else {
        out_rect = MathUtil::Rectangle<int>(0, (int)(params.height * params.res_scale_height),
                                            (int)(params.width * params.res_scale_width), 0);
    }

    return GetSurface(params, match_res_scale, load_if_create);
}

CachedSurface* RasterizerCacheOpenGL::GetTextureSurface(
    const Pica::TexturingRegs::FullTextureConfig& config) {

    Pica::Texture::TextureInfo info =
        Pica::Texture::TextureInfo::FromPicaRegister(config.config, config.format);

    CachedSurface params;
    params.addr = info.physical_address;
    params.width = info.width;
    params.height = info.height;
    params.is_tiled = true;
    params.pixel_format = CachedSurface::PixelFormatFromTextureFormat(info.format);
    return GetSurface(params, false, true);
}

// If the resolution
static u16 GetResolutionScaleFactor() {
    return !Settings::values.resolution_factor
               ? VideoCore::g_emu_window->GetFramebufferLayout().GetScalingRatio()
               : Settings::values.resolution_factor;
}

std::tuple<CachedSurface*, CachedSurface*, MathUtil::Rectangle<int>>
RasterizerCacheOpenGL::GetFramebufferSurfaces(
    const Pica::FramebufferRegs::FramebufferConfig& config) {

    const auto& regs = Pica::g_state.regs;

    // update resolution_scale_factor and reset cache if changed
    static u16 resolution_scale_factor = GetResolutionScaleFactor();
    if (resolution_scale_factor != GetResolutionScaleFactor()) {
        resolution_scale_factor = GetResolutionScaleFactor();
        FlushAll();
        InvalidateRegion(0, 0xffffffff, nullptr);
    }

    // Make sur that framebuffers don't overlap if both color and depth are being used
    u32 fb_area = config.GetWidth() * config.GetHeight();
    bool framebuffers_overlap =
        config.GetColorBufferPhysicalAddress() != 0 &&
        config.GetDepthBufferPhysicalAddress() != 0 &&
        MathUtil::IntervalsIntersect(
            config.GetColorBufferPhysicalAddress(),
            fb_area * GPU::Regs::BytesPerPixel(GPU::Regs::PixelFormat(config.color_format.Value())),
            config.GetDepthBufferPhysicalAddress(),
            fb_area * Pica::FramebufferRegs::BytesPerDepthPixel(config.depth_format));
    bool using_color_fb = config.GetColorBufferPhysicalAddress() != 0;
    bool depth_write_enable = regs.framebuffer.output_merger.depth_write_enable &&
                              regs.framebuffer.framebuffer.allow_depth_stencil_write;
    bool using_depth_fb = config.GetDepthBufferPhysicalAddress() != 0 &&
                          (regs.framebuffer.output_merger.depth_test_enable || depth_write_enable ||
                           !framebuffers_overlap);

    if (framebuffers_overlap && using_color_fb && using_depth_fb) {
        LOG_CRITICAL(Render_OpenGL, "Color and depth framebuffer memory regions overlap; "
                                    "overlapping framebuffers not supported!");
        using_depth_fb = false;
    }

    // get color and depth surfaces
    CachedSurface color_params;
    CachedSurface depth_params;
    color_params.width = depth_params.width = config.GetWidth();
    color_params.height = depth_params.height = config.GetHeight();
    color_params.is_tiled = depth_params.is_tiled = true;

    // Scale the resolution by the specified factor
    color_params.res_scale_width = resolution_scale_factor;
    depth_params.res_scale_width = resolution_scale_factor;
    color_params.res_scale_height = resolution_scale_factor;
    depth_params.res_scale_height = resolution_scale_factor;

    color_params.addr = config.GetColorBufferPhysicalAddress();
    color_params.pixel_format = CachedSurface::PixelFormatFromColorFormat(config.color_format);

    depth_params.addr = config.GetDepthBufferPhysicalAddress();
    depth_params.pixel_format = CachedSurface::PixelFormatFromDepthFormat(config.depth_format);

    MathUtil::Rectangle<int> color_rect;
    CachedSurface* color_surface =
        using_color_fb ? GetSurfaceRect(color_params, true, true, color_rect) : nullptr;

    MathUtil::Rectangle<int> depth_rect;
    CachedSurface* depth_surface =
        using_depth_fb ? GetSurfaceRect(depth_params, true, true, depth_rect) : nullptr;

    // Sanity check to make sure found surfaces aren't the same
    if (using_depth_fb && using_color_fb && color_surface == depth_surface) {
        LOG_CRITICAL(
            Render_OpenGL,
            "Color and depth framebuffer surfaces overlap; overlapping surfaces not supported!");
        using_depth_fb = false;
        depth_surface = nullptr;
    }

    MathUtil::Rectangle<int> rect;

    if (color_surface != nullptr && depth_surface != nullptr &&
        (depth_rect.left != color_rect.left || depth_rect.top != color_rect.top)) {
        // Can't specify separate color and depth viewport offsets in OpenGL, so re-zero both if
        // they don't match
        if (color_rect.left != 0 || color_rect.top != 0) {
            color_surface = GetSurface(color_params, true, true);
        }

        if (depth_rect.left != 0 || depth_rect.top != 0) {
            depth_surface = GetSurface(depth_params, true, true);
        }

        if (!color_surface->is_tiled) {
            rect = MathUtil::Rectangle<int>(
                0, 0, (int)(color_params.width * color_params.res_scale_width),
                (int)(color_params.height * color_params.res_scale_height));
        } else {
            rect = MathUtil::Rectangle<int>(
                0, (int)(color_params.height * color_params.res_scale_height),
                (int)(color_params.width * color_params.res_scale_width), 0);
        }
    } else if (color_surface != nullptr) {
        rect = color_rect;
    } else if (depth_surface != nullptr) {
        rect = depth_rect;
    } else {
        rect = MathUtil::Rectangle<int>(0, 0, 0, 0);
    }

    return std::make_tuple(color_surface, depth_surface, rect);
}

CachedSurface* RasterizerCacheOpenGL::TryGetFillSurface(const GPU::Regs::MemoryFillConfig& config) {
    auto surface_interval =
        boost::icl::interval<PAddr>::right_open(config.GetStartAddress(), config.GetEndAddress());
    auto range = surface_cache.equal_range(surface_interval);
    for (auto it = range.first; it != range.second; ++it) {
        for (auto it2 = it->second.begin(); it2 != it->second.end(); ++it2) {
            int bits_per_value = 0;
            if (config.fill_24bit) {
                bits_per_value = 24;
            } else if (config.fill_32bit) {
                bits_per_value = 32;
            } else {
                bits_per_value = 16;
            }

            CachedSurface* surface = it2->get();

            if (surface->addr == config.GetStartAddress() &&
                CachedSurface::GetFormatBpp(surface->pixel_format) == bits_per_value &&
                (surface->width * surface->height *
                 CachedSurface::GetFormatBpp(surface->pixel_format) / 8) ==
                    (config.GetEndAddress() - config.GetStartAddress())) {
                return surface;
            }
        }
    }

    return nullptr;
}

MICROPROFILE_DEFINE(OpenGL_SurfaceDownload, "OpenGL", "Surface Download", MP_RGB(128, 192, 64));
void RasterizerCacheOpenGL::FlushSurface(CachedSurface* surface) {
    using PixelFormat = CachedSurface::PixelFormat;
    using SurfaceType = CachedSurface::SurfaceType;

    if (!surface->dirty) {
        return;
    }

    MICROPROFILE_SCOPE(OpenGL_SurfaceDownload);

    u8* dst_buffer = Memory::GetPhysicalPointer(surface->addr);
    if (dst_buffer == nullptr) {
        return;
    }

    OpenGLState cur_state = OpenGLState::GetCurState();
    GLuint old_tex = cur_state.texture_units[0].texture_2d;

    OGLTexture unscaled_tex;
    GLuint texture_to_flush = surface->texture.handle;

    // If not 1x scale, blit scaled texture to a new 1x texture and use that to flush
    if (surface->res_scale_width != 1.f || surface->res_scale_height != 1.f) {
        unscaled_tex.Create();

        AllocateSurfaceTexture(unscaled_tex.handle, surface->pixel_format, surface->width,
                               surface->height);
        BlitTextures(
            surface->texture.handle, unscaled_tex.handle,
            CachedSurface::GetFormatType(surface->pixel_format),
            MathUtil::Rectangle<int>(0, 0, surface->GetScaledWidth(), surface->GetScaledHeight()),
            MathUtil::Rectangle<int>(0, 0, surface->width, surface->height));

        texture_to_flush = unscaled_tex.handle;
    }

    cur_state.texture_units[0].texture_2d = texture_to_flush;
    cur_state.Apply();
    glActiveTexture(GL_TEXTURE0);

    if (!surface->is_tiled) {
        // TODO: Ensure this will always be a color format, not a depth or other format
        ASSERT((size_t)surface->pixel_format < fb_format_tuples.size());
        const FormatTuple& tuple = fb_format_tuples[(unsigned int)surface->pixel_format];

        glPixelStorei(GL_PACK_ROW_LENGTH, (GLint)surface->pixel_stride);
        glGetTexImage(GL_TEXTURE_2D, 0, tuple.format, tuple.type, dst_buffer);
        glPixelStorei(GL_PACK_ROW_LENGTH, 0);
    } else {
        SurfaceType type = CachedSurface::GetFormatType(surface->pixel_format);
        if (type != SurfaceType::Depth && type != SurfaceType::DepthStencil) {
            ASSERT((size_t)surface->pixel_format < fb_format_tuples.size());
            const FormatTuple& tuple = fb_format_tuples[(unsigned int)surface->pixel_format];

            u32 bytes_per_pixel = CachedSurface::GetFormatBpp(surface->pixel_format) / 8;

            std::vector<u8> temp_gl_buffer(surface->width * surface->height * bytes_per_pixel);

            glGetTexImage(GL_TEXTURE_2D, 0, tuple.format, tuple.type, temp_gl_buffer.data());

            // Directly copy pixels. Internal OpenGL color formats are consistent so no conversion
            // is necessary.
            MortonCopyPixels(surface->pixel_format, surface->width, surface->height,
                             bytes_per_pixel, bytes_per_pixel, dst_buffer, temp_gl_buffer.data(),
                             false);
        } else {
            // Depth/Stencil formats need special treatment since they aren't sampleable using
            // LookupTexture and can't use RGBA format
            size_t tuple_idx = (size_t)surface->pixel_format - 14;
            ASSERT(tuple_idx < depth_format_tuples.size());
            const FormatTuple& tuple = depth_format_tuples[tuple_idx];

            u32 bytes_per_pixel = CachedSurface::GetFormatBpp(surface->pixel_format) / 8;

            // OpenGL needs 4 bpp alignment for D24 since using GL_UNSIGNED_INT as type
            bool use_4bpp = (surface->pixel_format == PixelFormat::D24);

            u32 gl_bytes_per_pixel = use_4bpp ? 4 : bytes_per_pixel;

            std::vector<u8> temp_gl_buffer(surface->width * surface->height * gl_bytes_per_pixel);

            glGetTexImage(GL_TEXTURE_2D, 0, tuple.format, tuple.type, temp_gl_buffer.data());

            u8* temp_gl_buffer_ptr = use_4bpp ? temp_gl_buffer.data() + 1 : temp_gl_buffer.data();

            MortonCopyPixels(surface->pixel_format, surface->width, surface->height,
                             bytes_per_pixel, gl_bytes_per_pixel, dst_buffer, temp_gl_buffer_ptr,
                             false);
        }
    }

    surface->dirty = false;

    cur_state.texture_units[0].texture_2d = old_tex;
    cur_state.Apply();
}

void RasterizerCacheOpenGL::FlushRegion(PAddr addr, u32 size, const CachedSurface* skip_surface,
                                        bool invalidate) {
    if (size == 0) {
        return;
    }

    // Gather up unique surfaces that touch the region
    std::unordered_set<std::shared_ptr<CachedSurface>> touching_surfaces;

    auto surface_interval = boost::icl::interval<PAddr>::right_open(addr, addr + size);
    auto cache_upper_bound = surface_cache.upper_bound(surface_interval);
    for (auto it = surface_cache.lower_bound(surface_interval); it != cache_upper_bound; ++it) {
        std::copy_if(it->second.begin(), it->second.end(),
                     std::inserter(touching_surfaces, touching_surfaces.end()),
                     [skip_surface](std::shared_ptr<CachedSurface> surface) {
                         return (surface.get() != skip_surface);
                     });
    }

    // Flush and invalidate surfaces
    for (auto surface : touching_surfaces) {
        FlushSurface(surface.get());
        if (invalidate) {
            Memory::RasterizerMarkRegionCached(surface->addr, surface->size, -1);
            surface_cache.subtract(
                std::make_pair(boost::icl::interval<PAddr>::right_open(
                                   surface->addr, surface->addr + surface->size),
                               std::set<std::shared_ptr<CachedSurface>>({surface})));
        }
    }
}

void RasterizerCacheOpenGL::FlushAll() {
    for (auto& surfaces : surface_cache) {
        for (auto& surface : surfaces.second) {
            FlushSurface(surface.get());
        }
    }
}
