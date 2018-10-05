// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <array>
#include <atomic>
#include <cstring>
#include <iterator>
#include <memory>
#include <optional>
#include <unordered_set>
#include <utility>
#include <vector>
#include <boost/range/iterator_range.hpp>
#include <glad/glad.h>
#include "common/alignment.h"
#include "common/bit_field.h"
#include "common/color.h"
#include "common/logging/log.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "common/vector_math.h"
#include "core/frontend/emu_window.h"
#include "core/memory.h"
#include "core/settings.h"
#include "video_core/pica_state.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/utils.h"
#include "video_core/video_core.h"

using SurfaceType = SurfaceParams::SurfaceType;
using PixelFormat = SurfaceParams::PixelFormat;

struct FormatTuple {
    GLint internal_format;
    GLenum format;
    GLenum type;
};

static constexpr std::array<FormatTuple, 5> fb_format_tuples = {{
    {GL_RGBA8, GL_RGBA, GL_UNSIGNED_INT_8_8_8_8},     // RGBA8
    {GL_RGB8, GL_BGR, GL_UNSIGNED_BYTE},              // RGB8
    {GL_RGB5_A1, GL_RGBA, GL_UNSIGNED_SHORT_5_5_5_1}, // RGB5A1
    {GL_RGB565, GL_RGB, GL_UNSIGNED_SHORT_5_6_5},     // RGB565
    {GL_RGBA4, GL_RGBA, GL_UNSIGNED_SHORT_4_4_4_4},   // RGBA4
}};

static constexpr std::array<FormatTuple, 4> depth_format_tuples = {{
    {GL_DEPTH_COMPONENT16, GL_DEPTH_COMPONENT, GL_UNSIGNED_SHORT}, // D16
    {},
    {GL_DEPTH_COMPONENT24, GL_DEPTH_COMPONENT, GL_UNSIGNED_INT},   // D24
    {GL_DEPTH24_STENCIL8, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8}, // D24S8
}};

static constexpr FormatTuple tex_tuple = {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};

static const FormatTuple& GetFormatTuple(PixelFormat pixel_format) {
    const SurfaceType type = SurfaceParams::GetFormatType(pixel_format);
    if (type == SurfaceType::Color) {
        ASSERT(static_cast<std::size_t>(pixel_format) < fb_format_tuples.size());
        return fb_format_tuples[static_cast<unsigned int>(pixel_format)];
    } else if (type == SurfaceType::Depth || type == SurfaceType::DepthStencil) {
        std::size_t tuple_idx = static_cast<std::size_t>(pixel_format) - 14;
        ASSERT(tuple_idx < depth_format_tuples.size());
        return depth_format_tuples[tuple_idx];
    }
    return tex_tuple;
}

template <typename Map, typename Interval>
constexpr auto RangeFromInterval(Map& map, const Interval& interval) {
    return boost::make_iterator_range(map.equal_range(interval));
}

static u16 GetResolutionScaleFactor() {
    return !Settings::values.resolution_factor
               ? VideoCore::g_renderer->GetRenderWindow().GetFramebufferLayout().GetScalingRatio()
               : Settings::values.resolution_factor;
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

    const u8* const buffer_end = tile_buffer + aligned_end - aligned_start;
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

// Allocate an uninitialized texture of appropriate size and format for the surface
static void AllocateSurfaceTexture(GLuint texture, const FormatTuple& format_tuple, u32 width,
                                   u32 height) {
    OpenGLState cur_state = OpenGLState::GetCurState();

    // Keep track of previous texture bindings
    GLuint old_tex = cur_state.texture_units[0].texture_2d;
    cur_state.texture_units[0].texture_2d = texture;
    cur_state.Apply();
    glActiveTexture(GL_TEXTURE0);

    glTexImage2D(GL_TEXTURE_2D, 0, format_tuple.internal_format, width, height, 0,
                 format_tuple.format, format_tuple.type, nullptr);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Restore previous texture bindings
    cur_state.texture_units[0].texture_2d = old_tex;
    cur_state.Apply();
}

static void AllocateTextureCube(GLuint texture, const FormatTuple& format_tuple, u32 width) {
    OpenGLState cur_state = OpenGLState::GetCurState();

    // Keep track of previous texture bindings
    GLuint old_tex = cur_state.texture_cube_unit.texture_cube;
    cur_state.texture_cube_unit.texture_cube = texture;
    cur_state.Apply();
    glActiveTexture(TextureUnits::TextureCube.Enum());

    for (auto faces : {
             GL_TEXTURE_CUBE_MAP_POSITIVE_X,
             GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
             GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
             GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
             GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
             GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
         }) {
        glTexImage2D(faces, 0, format_tuple.internal_format, width, width, 0, format_tuple.format,
                     format_tuple.type, nullptr);
    }

    // Restore previous texture bindings
    cur_state.texture_cube_unit.texture_cube = old_tex;
    cur_state.Apply();
}

static bool BlitTextures(GLuint src_tex, const MathUtil::Rectangle<u32>& src_rect, GLuint dst_tex,
                         const MathUtil::Rectangle<u32>& dst_rect, SurfaceType type,
                         GLuint read_fb_handle, GLuint draw_fb_handle) {
    OpenGLState prev_state = OpenGLState::GetCurState();
    SCOPE_EXIT({ prev_state.Apply(); });

    OpenGLState state;
    state.draw.read_framebuffer = read_fb_handle;
    state.draw.draw_framebuffer = draw_fb_handle;
    state.Apply();

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

    // TODO (wwylele): use GL_NEAREST for shadow map texture
    // Note: shadow map is treated as RGBA8 format in PICA, as well as in the rasterizer cache, but
    // doing linear intepolation componentwise would cause incorrect value. However, for a
    // well-programmed game this code path should be rarely executed for shadow map with
    // inconsistent scale.
    glBlitFramebuffer(src_rect.left, src_rect.bottom, src_rect.right, src_rect.top, dst_rect.left,
                      dst_rect.bottom, dst_rect.right, dst_rect.top, buffers,
                      buffers == GL_COLOR_BUFFER_BIT ? GL_LINEAR : GL_NEAREST);

    return true;
}

static bool FillSurface(const Surface& surface, const u8* fill_data,
                        const MathUtil::Rectangle<u32>& fill_rect, GLuint draw_fb_handle) {
    OpenGLState prev_state = OpenGLState::GetCurState();
    SCOPE_EXIT({ prev_state.Apply(); });

    OpenGLState state;
    state.scissor.enabled = true;
    state.scissor.x = static_cast<GLint>(fill_rect.left);
    state.scissor.y = static_cast<GLint>(fill_rect.bottom);
    state.scissor.width = static_cast<GLsizei>(fill_rect.GetWidth());
    state.scissor.height = static_cast<GLsizei>(fill_rect.GetHeight());

    state.draw.draw_framebuffer = draw_fb_handle;
    state.Apply();

    surface->InvalidateAllWatcher();

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
        std::memcpy(&value_32bit, fill_data, sizeof(u32));

        GLfloat value_float = (value_32bit & 0xFFFFFF) / 16777215.0f; // 2^24 - 1
        GLint value_int = (value_32bit >> 24);

        state.depth.write_mask = GL_TRUE;
        state.stencil.write_mask = -1;
        state.Apply();
        glClearBufferfi(GL_DEPTH_STENCIL, 0, value_float, value_int);
    }
    return true;
}

SurfaceParams SurfaceParams::FromInterval(SurfaceInterval interval) const {
    SurfaceParams params = *this;
    const u32 tiled_size = is_tiled ? 8 : 1;
    const u32 stride_tiled_bytes = BytesInPixels(stride * tiled_size);
    PAddr aligned_start =
        addr + Common::AlignDown(boost::icl::first(interval) - addr, stride_tiled_bytes);
    PAddr aligned_end =
        addr + Common::AlignUp(boost::icl::last_next(interval) - addr, stride_tiled_bytes);

    if (aligned_end - aligned_start > stride_tiled_bytes) {
        params.addr = aligned_start;
        params.height = (aligned_end - aligned_start) / BytesInPixels(stride);
    } else {
        // 1 row
        ASSERT(aligned_end - aligned_start == stride_tiled_bytes);
        const u32 tiled_alignment = BytesInPixels(is_tiled ? 8 * 8 : 1);
        aligned_start =
            addr + Common::AlignDown(boost::icl::first(interval) - addr, tiled_alignment);
        aligned_end =
            addr + Common::AlignUp(boost::icl::last_next(interval) - addr, tiled_alignment);
        params.addr = aligned_start;
        params.width = PixelsInBytes(aligned_end - aligned_start) / tiled_size;
        params.stride = params.width;
        params.height = tiled_size;
    }
    params.UpdateParams();

    return params;
}

SurfaceInterval SurfaceParams::GetSubRectInterval(MathUtil::Rectangle<u32> unscaled_rect) const {
    if (unscaled_rect.GetHeight() == 0 || unscaled_rect.GetWidth() == 0) {
        return {};
    }

    if (is_tiled) {
        unscaled_rect.left = Common::AlignDown(unscaled_rect.left, 8) * 8;
        unscaled_rect.bottom = Common::AlignDown(unscaled_rect.bottom, 8) / 8;
        unscaled_rect.right = Common::AlignUp(unscaled_rect.right, 8) * 8;
        unscaled_rect.top = Common::AlignUp(unscaled_rect.top, 8) / 8;
    }

    const u32 stride_tiled = !is_tiled ? stride : stride * 8;

    const u32 pixel_offset =
        stride_tiled * (!is_tiled ? unscaled_rect.bottom : (height / 8) - unscaled_rect.top) +
        unscaled_rect.left;

    const u32 pixels = (unscaled_rect.GetHeight() - 1) * stride_tiled + unscaled_rect.GetWidth();

    return {addr + BytesInPixels(pixel_offset), addr + BytesInPixels(pixel_offset + pixels)};
}

MathUtil::Rectangle<u32> SurfaceParams::GetSubRect(const SurfaceParams& sub_surface) const {
    const u32 begin_pixel_index = PixelsInBytes(sub_surface.addr - addr);

    if (is_tiled) {
        const int x0 = (begin_pixel_index % (stride * 8)) / 8;
        const int y0 = (begin_pixel_index / (stride * 8)) * 8;
        // Top to bottom
        return MathUtil::Rectangle<u32>(x0, height - y0, x0 + sub_surface.width,
                                        height - (y0 + sub_surface.height));
    }

    const int x0 = begin_pixel_index % stride;
    const int y0 = begin_pixel_index / stride;
    // Bottom to top
    return MathUtil::Rectangle<u32>(x0, y0 + sub_surface.height, x0 + sub_surface.width, y0);
}

MathUtil::Rectangle<u32> SurfaceParams::GetScaledSubRect(const SurfaceParams& sub_surface) const {
    auto rect = GetSubRect(sub_surface);
    rect.left = rect.left * res_scale;
    rect.right = rect.right * res_scale;
    rect.top = rect.top * res_scale;
    rect.bottom = rect.bottom * res_scale;
    return rect;
}

bool SurfaceParams::ExactMatch(const SurfaceParams& other_surface) const {
    return std::tie(other_surface.addr, other_surface.width, other_surface.height,
                    other_surface.stride, other_surface.pixel_format, other_surface.is_tiled) ==
               std::tie(addr, width, height, stride, pixel_format, is_tiled) &&
           pixel_format != PixelFormat::Invalid;
}

bool SurfaceParams::CanSubRect(const SurfaceParams& sub_surface) const {
    return sub_surface.addr >= addr && sub_surface.end <= end &&
           sub_surface.pixel_format == pixel_format && pixel_format != PixelFormat::Invalid &&
           sub_surface.is_tiled == is_tiled &&
           (sub_surface.addr - addr) % BytesInPixels(is_tiled ? 64 : 1) == 0 &&
           (sub_surface.stride == stride || sub_surface.height <= (is_tiled ? 8u : 1u)) &&
           GetSubRect(sub_surface).left + sub_surface.width <= stride;
}

bool SurfaceParams::CanExpand(const SurfaceParams& expanded_surface) const {
    return pixel_format != PixelFormat::Invalid && pixel_format == expanded_surface.pixel_format &&
           addr <= expanded_surface.end && expanded_surface.addr <= end &&
           is_tiled == expanded_surface.is_tiled && stride == expanded_surface.stride &&
           (std::max(expanded_surface.addr, addr) - std::min(expanded_surface.addr, addr)) %
                   BytesInPixels(stride * (is_tiled ? 8 : 1)) ==
               0;
}

bool SurfaceParams::CanTexCopy(const SurfaceParams& texcopy_params) const {
    if (pixel_format == PixelFormat::Invalid || addr > texcopy_params.addr ||
        end < texcopy_params.end) {
        return false;
    }
    if (texcopy_params.width != texcopy_params.stride) {
        const u32 tile_stride = BytesInPixels(stride * (is_tiled ? 8 : 1));
        return (texcopy_params.addr - addr) % BytesInPixels(is_tiled ? 64 : 1) == 0 &&
               texcopy_params.width % BytesInPixels(is_tiled ? 64 : 1) == 0 &&
               (texcopy_params.height == 1 || texcopy_params.stride == tile_stride) &&
               ((texcopy_params.addr - addr) % tile_stride) + texcopy_params.width <= tile_stride;
    }
    return FromInterval(texcopy_params.GetInterval()).GetInterval() == texcopy_params.GetInterval();
}

bool CachedSurface::CanFill(const SurfaceParams& dest_surface,
                            SurfaceInterval fill_interval) const {
    if (type == SurfaceType::Fill && IsRegionValid(fill_interval) &&
        boost::icl::first(fill_interval) >= addr &&
        boost::icl::last_next(fill_interval) <= end && // dest_surface is within our fill range
        dest_surface.FromInterval(fill_interval).GetInterval() ==
            fill_interval) { // make sure interval is a rectangle in dest surface
        if (fill_size * 8 != dest_surface.GetFormatBpp()) {
            // Check if bits repeat for our fill_size
            const u32 dest_bytes_per_pixel = std::max(dest_surface.GetFormatBpp() / 8, 1u);
            std::vector<u8> fill_test(fill_size * dest_bytes_per_pixel);

            for (u32 i = 0; i < dest_bytes_per_pixel; ++i)
                std::memcpy(&fill_test[i * fill_size], &fill_data[0], fill_size);

            for (u32 i = 0; i < fill_size; ++i)
                if (std::memcmp(&fill_test[dest_bytes_per_pixel * i], &fill_test[0],
                                dest_bytes_per_pixel) != 0)
                    return false;

            if (dest_surface.GetFormatBpp() == 4 && (fill_test[0] & 0xF) != (fill_test[0] >> 4))
                return false;
        }
        return true;
    }
    return false;
}

bool CachedSurface::CanCopy(const SurfaceParams& dest_surface,
                            SurfaceInterval copy_interval) const {
    SurfaceParams subrect_params = dest_surface.FromInterval(copy_interval);
    ASSERT(subrect_params.GetInterval() == copy_interval);
    if (CanSubRect(subrect_params))
        return true;

    if (CanFill(dest_surface, copy_interval))
        return true;

    return false;
}

SurfaceInterval SurfaceParams::GetCopyableInterval(const Surface& src_surface) const {
    SurfaceInterval result{};
    const auto valid_regions =
        SurfaceRegions(GetInterval() & src_surface->GetInterval()) - src_surface->invalid_regions;
    for (auto& valid_interval : valid_regions) {
        const SurfaceInterval aligned_interval{
            addr + Common::AlignUp(boost::icl::first(valid_interval) - addr,
                                   BytesInPixels(is_tiled ? 8 * 8 : 1)),
            addr + Common::AlignDown(boost::icl::last_next(valid_interval) - addr,
                                     BytesInPixels(is_tiled ? 8 * 8 : 1))};

        if (BytesInPixels(is_tiled ? 8 * 8 : 1) > boost::icl::length(valid_interval) ||
            boost::icl::length(aligned_interval) == 0) {
            continue;
        }

        // Get the rectangle within aligned_interval
        const u32 stride_bytes = BytesInPixels(stride) * (is_tiled ? 8 : 1);
        SurfaceInterval rect_interval{
            addr + Common::AlignUp(boost::icl::first(aligned_interval) - addr, stride_bytes),
            addr + Common::AlignDown(boost::icl::last_next(aligned_interval) - addr, stride_bytes),
        };
        if (boost::icl::first(rect_interval) > boost::icl::last_next(rect_interval)) {
            // 1 row
            rect_interval = aligned_interval;
        } else if (boost::icl::length(rect_interval) == 0) {
            // 2 rows that do not make a rectangle, return the larger one
            const SurfaceInterval row1{boost::icl::first(aligned_interval),
                                       boost::icl::first(rect_interval)};
            const SurfaceInterval row2{boost::icl::first(rect_interval),
                                       boost::icl::last_next(aligned_interval)};
            rect_interval = (boost::icl::length(row1) > boost::icl::length(row2)) ? row1 : row2;
        }

        if (boost::icl::length(rect_interval) > boost::icl::length(result)) {
            result = rect_interval;
        }
    }
    return result;
}

void RasterizerCacheOpenGL::CopySurface(const Surface& src_surface, const Surface& dst_surface,
                                        SurfaceInterval copy_interval) {
    SurfaceParams subrect_params = dst_surface->FromInterval(copy_interval);
    ASSERT(subrect_params.GetInterval() == copy_interval);

    ASSERT(src_surface != dst_surface);

    // This is only called when CanCopy is true, no need to run checks here
    if (src_surface->type == SurfaceType::Fill) {
        // FillSurface needs a 4 bytes buffer
        const u32 fill_offset =
            (boost::icl::first(copy_interval) - src_surface->addr) % src_surface->fill_size;
        std::array<u8, 4> fill_buffer;

        u32 fill_buff_pos = fill_offset;
        for (int i : {0, 1, 2, 3})
            fill_buffer[i] = src_surface->fill_data[fill_buff_pos++ % src_surface->fill_size];

        FillSurface(dst_surface, &fill_buffer[0], dst_surface->GetScaledSubRect(subrect_params),
                    draw_framebuffer.handle);
        return;
    }
    if (src_surface->CanSubRect(subrect_params)) {
        BlitTextures(src_surface->texture.handle, src_surface->GetScaledSubRect(subrect_params),
                     dst_surface->texture.handle, dst_surface->GetScaledSubRect(subrect_params),
                     src_surface->type, read_framebuffer.handle, draw_framebuffer.handle);
        return;
    }
    UNREACHABLE();
}

MICROPROFILE_DEFINE(OpenGL_SurfaceLoad, "OpenGL", "Surface Load", MP_RGB(128, 64, 192));
void CachedSurface::LoadGLBuffer(PAddr load_start, PAddr load_end) {
    ASSERT(type != SurfaceType::Fill);

    const u8* const texture_src_data = Memory::GetPhysicalPointer(addr);
    if (texture_src_data == nullptr)
        return;

    if (gl_buffer == nullptr) {
        gl_buffer_size = width * height * GetGLBytesPerPixel(pixel_format);
        gl_buffer.reset(new u8[gl_buffer_size]);
    }

    // TODO: Should probably be done in ::Memory:: and check for other regions too
    if (load_start < Memory::VRAM_VADDR_END && load_end > Memory::VRAM_VADDR_END)
        load_end = Memory::VRAM_VADDR_END;

    if (load_start < Memory::VRAM_VADDR && load_end > Memory::VRAM_VADDR)
        load_start = Memory::VRAM_VADDR;

    MICROPROFILE_SCOPE(OpenGL_SurfaceLoad);

    ASSERT(load_start >= addr && load_end <= end);
    const u32 start_offset = load_start - addr;

    if (!is_tiled) {
        ASSERT(type == SurfaceType::Color);
        std::memcpy(&gl_buffer[start_offset], texture_src_data + start_offset,
                    load_end - load_start);
    } else {
        if (type == SurfaceType::Texture) {
            Pica::Texture::TextureInfo tex_info{};
            tex_info.width = width;
            tex_info.height = height;
            tex_info.format = static_cast<Pica::TexturingRegs::TextureFormat>(pixel_format);
            tex_info.SetDefaultStride();
            tex_info.physical_address = addr;

            const SurfaceInterval load_interval(load_start, load_end);
            const auto rect = GetSubRect(FromInterval(load_interval));
            ASSERT(FromInterval(load_interval).GetInterval() == load_interval);

            for (unsigned y = rect.bottom; y < rect.top; ++y) {
                for (unsigned x = rect.left; x < rect.right; ++x) {
                    auto vec4 =
                        Pica::Texture::LookupTexture(texture_src_data, x, height - 1 - y, tex_info);
                    const std::size_t offset = (x + (width * y)) * 4;
                    std::memcpy(&gl_buffer[offset], vec4.AsArray(), 4);
                }
            }
        } else {
            morton_to_gl_fns[static_cast<std::size_t>(pixel_format)](stride, height, &gl_buffer[0],
                                                                     addr, load_start, load_end);
        }
    }
}

MICROPROFILE_DEFINE(OpenGL_SurfaceFlush, "OpenGL", "Surface Flush", MP_RGB(128, 192, 64));
void CachedSurface::FlushGLBuffer(PAddr flush_start, PAddr flush_end) {
    u8* const dst_buffer = Memory::GetPhysicalPointer(addr);
    if (dst_buffer == nullptr)
        return;

    ASSERT(gl_buffer_size == width * height * GetGLBytesPerPixel(pixel_format));

    // TODO: Should probably be done in ::Memory:: and check for other regions too
    // same as loadglbuffer()
    if (flush_start < Memory::VRAM_VADDR_END && flush_end > Memory::VRAM_VADDR_END)
        flush_end = Memory::VRAM_VADDR_END;

    if (flush_start < Memory::VRAM_VADDR && flush_end > Memory::VRAM_VADDR)
        flush_start = Memory::VRAM_VADDR;

    MICROPROFILE_SCOPE(OpenGL_SurfaceFlush);

    ASSERT(flush_start >= addr && flush_end <= end);
    const u32 start_offset = flush_start - addr;
    const u32 end_offset = flush_end - addr;

    if (type == SurfaceType::Fill) {
        const u32 coarse_start_offset = start_offset - (start_offset % fill_size);
        const u32 backup_bytes = start_offset % fill_size;
        std::array<u8, 4> backup_data;
        if (backup_bytes)
            std::memcpy(&backup_data[0], &dst_buffer[coarse_start_offset], backup_bytes);

        for (u32 offset = coarse_start_offset; offset < end_offset; offset += fill_size) {
            std::memcpy(&dst_buffer[offset], &fill_data[0],
                        std::min(fill_size, end_offset - offset));
        }

        if (backup_bytes)
            std::memcpy(&dst_buffer[coarse_start_offset], &backup_data[0], backup_bytes);
    } else if (!is_tiled) {
        ASSERT(type == SurfaceType::Color);
        std::memcpy(dst_buffer + start_offset, &gl_buffer[start_offset], flush_end - flush_start);
    } else {
        gl_to_morton_fns[static_cast<std::size_t>(pixel_format)](stride, height, &gl_buffer[0],
                                                                 addr, flush_start, flush_end);
    }
}

MICROPROFILE_DEFINE(OpenGL_TextureUL, "OpenGL", "Texture Upload", MP_RGB(128, 64, 192));
void CachedSurface::UploadGLTexture(const MathUtil::Rectangle<u32>& rect, GLuint read_fb_handle,
                                    GLuint draw_fb_handle) {
    if (type == SurfaceType::Fill)
        return;

    MICROPROFILE_SCOPE(OpenGL_TextureUL);

    ASSERT(gl_buffer_size == width * height * GetGLBytesPerPixel(pixel_format));

    // Load data from memory to the surface
    GLint x0 = static_cast<GLint>(rect.left);
    GLint y0 = static_cast<GLint>(rect.bottom);
    std::size_t buffer_offset = (y0 * stride + x0) * GetGLBytesPerPixel(pixel_format);

    const FormatTuple& tuple = GetFormatTuple(pixel_format);
    GLuint target_tex = texture.handle;

    // If not 1x scale, create 1x texture that we will blit from to replace texture subrect in
    // surface
    OGLTexture unscaled_tex;
    if (res_scale != 1) {
        x0 = 0;
        y0 = 0;

        unscaled_tex.Create();
        AllocateSurfaceTexture(unscaled_tex.handle, tuple, rect.GetWidth(), rect.GetHeight());
        target_tex = unscaled_tex.handle;
    }

    OpenGLState cur_state = OpenGLState::GetCurState();

    GLuint old_tex = cur_state.texture_units[0].texture_2d;
    cur_state.texture_units[0].texture_2d = target_tex;
    cur_state.Apply();

    // Ensure no bad interactions with GL_UNPACK_ALIGNMENT
    ASSERT(stride * GetGLBytesPerPixel(pixel_format) % 4 == 0);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(stride));

    glActiveTexture(GL_TEXTURE0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, x0, y0, static_cast<GLsizei>(rect.GetWidth()),
                    static_cast<GLsizei>(rect.GetHeight()), tuple.format, tuple.type,
                    &gl_buffer[buffer_offset]);

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);

    cur_state.texture_units[0].texture_2d = old_tex;
    cur_state.Apply();

    if (res_scale != 1) {
        auto scaled_rect = rect;
        scaled_rect.left *= res_scale;
        scaled_rect.top *= res_scale;
        scaled_rect.right *= res_scale;
        scaled_rect.bottom *= res_scale;

        BlitTextures(unscaled_tex.handle, {0, rect.GetHeight(), rect.GetWidth(), 0}, texture.handle,
                     scaled_rect, type, read_fb_handle, draw_fb_handle);
    }

    InvalidateAllWatcher();
}

MICROPROFILE_DEFINE(OpenGL_TextureDL, "OpenGL", "Texture Download", MP_RGB(128, 192, 64));
void CachedSurface::DownloadGLTexture(const MathUtil::Rectangle<u32>& rect, GLuint read_fb_handle,
                                      GLuint draw_fb_handle) {
    if (type == SurfaceType::Fill)
        return;

    MICROPROFILE_SCOPE(OpenGL_TextureDL);

    if (gl_buffer == nullptr) {
        gl_buffer_size = width * height * GetGLBytesPerPixel(pixel_format);
        gl_buffer.reset(new u8[gl_buffer_size]);
    }

    OpenGLState state = OpenGLState::GetCurState();
    OpenGLState prev_state = state;
    SCOPE_EXIT({ prev_state.Apply(); });

    const FormatTuple& tuple = GetFormatTuple(pixel_format);

    // Ensure no bad interactions with GL_PACK_ALIGNMENT
    ASSERT(stride * GetGLBytesPerPixel(pixel_format) % 4 == 0);
    glPixelStorei(GL_PACK_ROW_LENGTH, static_cast<GLint>(stride));
    std::size_t buffer_offset =
        (rect.bottom * stride + rect.left) * GetGLBytesPerPixel(pixel_format);

    // If not 1x scale, blit scaled texture to a new 1x texture and use that to flush
    if (res_scale != 1) {
        auto scaled_rect = rect;
        scaled_rect.left *= res_scale;
        scaled_rect.top *= res_scale;
        scaled_rect.right *= res_scale;
        scaled_rect.bottom *= res_scale;

        OGLTexture unscaled_tex;
        unscaled_tex.Create();

        MathUtil::Rectangle<u32> unscaled_tex_rect{0, rect.GetHeight(), rect.GetWidth(), 0};
        AllocateSurfaceTexture(unscaled_tex.handle, tuple, rect.GetWidth(), rect.GetHeight());
        BlitTextures(texture.handle, scaled_rect, unscaled_tex.handle, unscaled_tex_rect, type,
                     read_fb_handle, draw_fb_handle);

        state.texture_units[0].texture_2d = unscaled_tex.handle;
        state.Apply();

        glActiveTexture(GL_TEXTURE0);
        glGetTexImage(GL_TEXTURE_2D, 0, tuple.format, tuple.type, &gl_buffer[buffer_offset]);
    } else {
        state.ResetTexture(texture.handle);
        state.draw.read_framebuffer = read_fb_handle;
        state.Apply();

        if (type == SurfaceType::Color || type == SurfaceType::Texture) {
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   texture.handle, 0);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                   0, 0);
        } else if (type == SurfaceType::Depth) {
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D,
                                   texture.handle, 0);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
        } else {
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                   texture.handle, 0);
        }
        glReadPixels(static_cast<GLint>(rect.left), static_cast<GLint>(rect.bottom),
                     static_cast<GLsizei>(rect.GetWidth()), static_cast<GLsizei>(rect.GetHeight()),
                     tuple.format, tuple.type, &gl_buffer[buffer_offset]);
    }

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
}

enum MatchFlags {
    Invalid = 1,      // Flag that can be applied to other match types, invalid matches require
                      // validation before they can be used
    Exact = 1 << 1,   // Surfaces perfectly match
    SubRect = 1 << 2, // Surface encompasses params
    Copy = 1 << 3,    // Surface we can copy from
    Expand = 1 << 4,  // Surface that can expand params
    TexCopy = 1 << 5  // Surface that will match a display transfer "texture copy" parameters
};

constexpr MatchFlags operator|(MatchFlags lhs, MatchFlags rhs) {
    return static_cast<MatchFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

/// Get the best surface match (and its match type) for the given flags
template <MatchFlags find_flags>
Surface FindMatch(const SurfaceCache& surface_cache, const SurfaceParams& params,
                  ScaleMatch match_scale_type,
                  std::optional<SurfaceInterval> validate_interval = {}) {
    Surface match_surface = nullptr;
    bool match_valid = false;
    u32 match_scale = 0;
    SurfaceInterval match_interval{};

    for (auto& pair : RangeFromInterval(surface_cache, params.GetInterval())) {
        for (auto& surface : pair.second) {
            bool res_scale_matched = match_scale_type == ScaleMatch::Exact
                                         ? (params.res_scale == surface->res_scale)
                                         : (params.res_scale <= surface->res_scale);
            // validity will be checked in GetCopyableInterval
            bool is_valid =
                find_flags & MatchFlags::Copy
                    ? true
                    : surface->IsRegionValid(validate_interval.value_or(params.GetInterval()));

            if (!(find_flags & MatchFlags::Invalid) && !is_valid)
                continue;

            auto IsMatch_Helper = [&](auto check_type, auto match_fn) {
                if (!(find_flags & check_type))
                    return;

                bool matched;
                SurfaceInterval surface_interval;
                std::tie(matched, surface_interval) = match_fn();
                if (!matched)
                    return;

                if (!res_scale_matched && match_scale_type != ScaleMatch::Ignore &&
                    surface->type != SurfaceType::Fill)
                    return;

                // Found a match, update only if this is better than the previous one
                auto UpdateMatch = [&] {
                    match_surface = surface;
                    match_valid = is_valid;
                    match_scale = surface->res_scale;
                    match_interval = surface_interval;
                };

                if (surface->res_scale > match_scale) {
                    UpdateMatch();
                    return;
                } else if (surface->res_scale < match_scale) {
                    return;
                }

                if (is_valid && !match_valid) {
                    UpdateMatch();
                    return;
                } else if (is_valid != match_valid) {
                    return;
                }

                if (boost::icl::length(surface_interval) > boost::icl::length(match_interval)) {
                    UpdateMatch();
                }
            };
            IsMatch_Helper(std::integral_constant<MatchFlags, MatchFlags::Exact>{}, [&] {
                return std::make_pair(surface->ExactMatch(params), surface->GetInterval());
            });
            IsMatch_Helper(std::integral_constant<MatchFlags, MatchFlags::SubRect>{}, [&] {
                return std::make_pair(surface->CanSubRect(params), surface->GetInterval());
            });
            IsMatch_Helper(std::integral_constant<MatchFlags, MatchFlags::Copy>{}, [&] {
                ASSERT(validate_interval);
                auto copy_interval =
                    params.FromInterval(*validate_interval).GetCopyableInterval(surface);
                bool matched = boost::icl::length(copy_interval & *validate_interval) != 0 &&
                               surface->CanCopy(params, copy_interval);
                return std::make_pair(matched, copy_interval);
            });
            IsMatch_Helper(std::integral_constant<MatchFlags, MatchFlags::Expand>{}, [&] {
                return std::make_pair(surface->CanExpand(params), surface->GetInterval());
            });
            IsMatch_Helper(std::integral_constant<MatchFlags, MatchFlags::TexCopy>{}, [&] {
                return std::make_pair(surface->CanTexCopy(params), surface->GetInterval());
            });
        }
    }
    return match_surface;
}

RasterizerCacheOpenGL::RasterizerCacheOpenGL() {
    read_framebuffer.Create();
    draw_framebuffer.Create();

    attributeless_vao.Create();

    d24s8_abgr_buffer.Create();
    d24s8_abgr_buffer_size = 0;

    const char* vs_source = R"(
#version 330 core
const vec2 vertices[4] = vec2[4](vec2(-1.0, -1.0), vec2(1.0, -1.0), vec2(-1.0, 1.0), vec2(1.0, 1.0));
void main() {
    gl_Position = vec4(vertices[gl_VertexID], 0.0, 1.0);
}
)";
    const char* fs_source = R"(
#version 330 core

uniform samplerBuffer tbo;
uniform vec2 tbo_size;
uniform vec4 viewport;

out vec4 color;

void main() {
    vec2 tbo_coord = (gl_FragCoord.xy - viewport.xy) * tbo_size / viewport.zw;
    int tbo_offset = int(tbo_coord.y) * int(tbo_size.x) + int(tbo_coord.x);
    color = texelFetch(tbo, tbo_offset).rabg;
}
)";
    d24s8_abgr_shader.Create(vs_source, fs_source);

    OpenGLState state = OpenGLState::GetCurState();
    GLuint old_program = state.draw.shader_program;
    state.draw.shader_program = d24s8_abgr_shader.handle;
    state.Apply();

    GLint tbo_u_id = glGetUniformLocation(d24s8_abgr_shader.handle, "tbo");
    ASSERT(tbo_u_id != -1);
    glUniform1i(tbo_u_id, 0);

    state.draw.shader_program = old_program;
    state.Apply();

    d24s8_abgr_tbo_size_u_id = glGetUniformLocation(d24s8_abgr_shader.handle, "tbo_size");
    ASSERT(d24s8_abgr_tbo_size_u_id != -1);
    d24s8_abgr_viewport_u_id = glGetUniformLocation(d24s8_abgr_shader.handle, "viewport");
    ASSERT(d24s8_abgr_viewport_u_id != -1);
}

RasterizerCacheOpenGL::~RasterizerCacheOpenGL() {
    FlushAll();
    while (!surface_cache.empty())
        UnregisterSurface(*surface_cache.begin()->second.begin());
}

bool RasterizerCacheOpenGL::BlitSurfaces(const Surface& src_surface,
                                         const MathUtil::Rectangle<u32>& src_rect,
                                         const Surface& dst_surface,
                                         const MathUtil::Rectangle<u32>& dst_rect) {
    if (!SurfaceParams::CheckFormatsBlittable(src_surface->pixel_format, dst_surface->pixel_format))
        return false;

    dst_surface->InvalidateAllWatcher();

    return BlitTextures(src_surface->texture.handle, src_rect, dst_surface->texture.handle,
                        dst_rect, src_surface->type, read_framebuffer.handle,
                        draw_framebuffer.handle);
}

void RasterizerCacheOpenGL::ConvertD24S8toABGR(GLuint src_tex,
                                               const MathUtil::Rectangle<u32>& src_rect,
                                               GLuint dst_tex,
                                               const MathUtil::Rectangle<u32>& dst_rect) {
    OpenGLState prev_state = OpenGLState::GetCurState();
    SCOPE_EXIT({ prev_state.Apply(); });

    OpenGLState state;
    state.draw.read_framebuffer = read_framebuffer.handle;
    state.draw.draw_framebuffer = draw_framebuffer.handle;
    state.Apply();

    glBindBuffer(GL_PIXEL_PACK_BUFFER, d24s8_abgr_buffer.handle);

    GLsizeiptr target_pbo_size = src_rect.GetWidth() * src_rect.GetHeight() * 4;
    if (target_pbo_size > d24s8_abgr_buffer_size) {
        d24s8_abgr_buffer_size = target_pbo_size * 2;
        glBufferData(GL_PIXEL_PACK_BUFFER, d24s8_abgr_buffer_size, nullptr, GL_STREAM_COPY);
    }

    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, 0, 0);
    glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, src_tex,
                           0);
    glReadPixels(static_cast<GLint>(src_rect.left), static_cast<GLint>(src_rect.bottom),
                 static_cast<GLsizei>(src_rect.GetWidth()),
                 static_cast<GLsizei>(src_rect.GetHeight()), GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8,
                 0);

    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);

    // PBO now contains src_tex in RABG format
    state.draw.shader_program = d24s8_abgr_shader.handle;
    state.draw.vertex_array = attributeless_vao.handle;
    state.viewport.x = static_cast<GLint>(dst_rect.left);
    state.viewport.y = static_cast<GLint>(dst_rect.bottom);
    state.viewport.width = static_cast<GLsizei>(dst_rect.GetWidth());
    state.viewport.height = static_cast<GLsizei>(dst_rect.GetHeight());
    state.Apply();

    OGLTexture tbo;
    tbo.Create();
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_BUFFER, tbo.handle);
    glTexBuffer(GL_TEXTURE_BUFFER, GL_RGBA8, d24s8_abgr_buffer.handle);

    glUniform2f(d24s8_abgr_tbo_size_u_id, static_cast<GLfloat>(src_rect.GetWidth()),
                static_cast<GLfloat>(src_rect.GetHeight()));
    glUniform4f(d24s8_abgr_viewport_u_id, static_cast<GLfloat>(state.viewport.x),
                static_cast<GLfloat>(state.viewport.y), static_cast<GLfloat>(state.viewport.width),
                static_cast<GLfloat>(state.viewport.height));

    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, dst_tex, 0);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D, 0, 0);
    glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

    glBindTexture(GL_TEXTURE_BUFFER, 0);
}

Surface RasterizerCacheOpenGL::GetSurface(const SurfaceParams& params, ScaleMatch match_res_scale,
                                          bool load_if_create) {
    if (params.addr == 0 || params.height * params.width == 0) {
        return nullptr;
    }
    // Use GetSurfaceSubRect instead
    ASSERT(params.width == params.stride);

    ASSERT(!params.is_tiled || (params.width % 8 == 0 && params.height % 8 == 0));

    // Check for an exact match in existing surfaces
    Surface surface =
        FindMatch<MatchFlags::Exact | MatchFlags::Invalid>(surface_cache, params, match_res_scale);

    if (surface == nullptr) {
        u16 target_res_scale = params.res_scale;
        if (match_res_scale != ScaleMatch::Exact) {
            // This surface may have a subrect of another surface with a higher res_scale, find it
            // to adjust our params
            SurfaceParams find_params = params;
            Surface expandable = FindMatch<MatchFlags::Expand | MatchFlags::Invalid>(
                surface_cache, find_params, match_res_scale);
            if (expandable != nullptr && expandable->res_scale > target_res_scale) {
                target_res_scale = expandable->res_scale;
            }
            // Keep res_scale when reinterpreting d24s8 -> rgba8
            if (params.pixel_format == PixelFormat::RGBA8) {
                find_params.pixel_format = PixelFormat::D24S8;
                expandable = FindMatch<MatchFlags::Expand | MatchFlags::Invalid>(
                    surface_cache, find_params, match_res_scale);
                if (expandable != nullptr && expandable->res_scale > target_res_scale) {
                    target_res_scale = expandable->res_scale;
                }
            }
        }
        SurfaceParams new_params = params;
        new_params.res_scale = target_res_scale;
        surface = CreateSurface(new_params);
        RegisterSurface(surface);
    }

    if (load_if_create) {
        ValidateSurface(surface, params.addr, params.size);
    }

    return surface;
}

SurfaceRect_Tuple RasterizerCacheOpenGL::GetSurfaceSubRect(const SurfaceParams& params,
                                                           ScaleMatch match_res_scale,
                                                           bool load_if_create) {
    if (params.addr == 0 || params.height * params.width == 0) {
        return std::make_tuple(nullptr, MathUtil::Rectangle<u32>{});
    }

    // Attempt to find encompassing surface
    Surface surface = FindMatch<MatchFlags::SubRect | MatchFlags::Invalid>(surface_cache, params,
                                                                           match_res_scale);

    // Check if FindMatch failed because of res scaling
    // If that's the case create a new surface with
    // the dimensions of the lower res_scale surface
    // to suggest it should not be used again
    if (surface == nullptr && match_res_scale != ScaleMatch::Ignore) {
        surface = FindMatch<MatchFlags::SubRect | MatchFlags::Invalid>(surface_cache, params,
                                                                       ScaleMatch::Ignore);
        if (surface != nullptr) {
            ASSERT(surface->res_scale < params.res_scale);
            SurfaceParams new_params = *surface;
            new_params.res_scale = params.res_scale;

            surface = CreateSurface(new_params);
            RegisterSurface(surface);
        }
    }

    SurfaceParams aligned_params = params;
    if (params.is_tiled) {
        aligned_params.height = Common::AlignUp(params.height, 8);
        aligned_params.width = Common::AlignUp(params.width, 8);
        aligned_params.stride = Common::AlignUp(params.stride, 8);
        aligned_params.UpdateParams();
    }

    // Check for a surface we can expand before creating a new one
    if (surface == nullptr) {
        surface = FindMatch<MatchFlags::Expand | MatchFlags::Invalid>(surface_cache, aligned_params,
                                                                      match_res_scale);
        if (surface != nullptr) {
            aligned_params.width = aligned_params.stride;
            aligned_params.UpdateParams();

            SurfaceParams new_params = *surface;
            new_params.addr = std::min(aligned_params.addr, surface->addr);
            new_params.end = std::max(aligned_params.end, surface->end);
            new_params.size = new_params.end - new_params.addr;
            new_params.height =
                new_params.size / aligned_params.BytesInPixels(aligned_params.stride);
            ASSERT(new_params.size % aligned_params.BytesInPixels(aligned_params.stride) == 0);

            Surface new_surface = CreateSurface(new_params);
            DuplicateSurface(surface, new_surface);

            // Delete the expanded surface, this can't be done safely yet
            // because it may still be in use
            remove_surfaces.emplace(surface);

            surface = new_surface;
            RegisterSurface(new_surface);
        }
    }

    // No subrect found - create and return a new surface
    if (surface == nullptr) {
        SurfaceParams new_params = aligned_params;
        // Can't have gaps in a surface
        new_params.width = aligned_params.stride;
        new_params.UpdateParams();
        // GetSurface will create the new surface and possibly adjust res_scale if necessary
        surface = GetSurface(new_params, match_res_scale, load_if_create);
    } else if (load_if_create) {
        ValidateSurface(surface, aligned_params.addr, aligned_params.size);
    }

    return std::make_tuple(surface, surface->GetScaledSubRect(params));
}

Surface RasterizerCacheOpenGL::GetTextureSurface(
    const Pica::TexturingRegs::FullTextureConfig& config) {
    Pica::Texture::TextureInfo info =
        Pica::Texture::TextureInfo::FromPicaRegister(config.config, config.format);
    return GetTextureSurface(info);
}

Surface RasterizerCacheOpenGL::GetTextureSurface(const Pica::Texture::TextureInfo& info) {
    SurfaceParams params;
    params.addr = info.physical_address;
    params.width = info.width;
    params.height = info.height;
    params.is_tiled = true;
    params.pixel_format = SurfaceParams::PixelFormatFromTextureFormat(info.format);
    params.UpdateParams();

    if (info.width % 8 != 0 || info.height % 8 != 0) {
        Surface src_surface;
        MathUtil::Rectangle<u32> rect;
        std::tie(src_surface, rect) = GetSurfaceSubRect(params, ScaleMatch::Ignore, true);

        params.res_scale = src_surface->res_scale;
        Surface tmp_surface = CreateSurface(params);
        BlitTextures(src_surface->texture.handle, rect, tmp_surface->texture.handle,
                     tmp_surface->GetScaledRect(),
                     SurfaceParams::GetFormatType(params.pixel_format), read_framebuffer.handle,
                     draw_framebuffer.handle);

        remove_surfaces.emplace(tmp_surface);
        return tmp_surface;
    }

    return GetSurface(params, ScaleMatch::Ignore, true);
}

const CachedTextureCube& RasterizerCacheOpenGL::GetTextureCube(const TextureCubeConfig& config) {
    auto& cube = texture_cube_cache[config];

    struct Face {
        Face(std::shared_ptr<SurfaceWatcher>& watcher, PAddr address, GLenum gl_face)
            : watcher(watcher), address(address), gl_face(gl_face) {}
        std::shared_ptr<SurfaceWatcher>& watcher;
        PAddr address;
        GLenum gl_face;
    };

    const std::array<Face, 6> faces{{
        {cube.px, config.px, GL_TEXTURE_CUBE_MAP_POSITIVE_X},
        {cube.nx, config.nx, GL_TEXTURE_CUBE_MAP_NEGATIVE_X},
        {cube.py, config.py, GL_TEXTURE_CUBE_MAP_POSITIVE_Y},
        {cube.ny, config.ny, GL_TEXTURE_CUBE_MAP_NEGATIVE_Y},
        {cube.pz, config.pz, GL_TEXTURE_CUBE_MAP_POSITIVE_Z},
        {cube.nz, config.nz, GL_TEXTURE_CUBE_MAP_NEGATIVE_Z},
    }};

    for (const Face& face : faces) {
        if (!face.watcher || !face.watcher->Get()) {
            Pica::Texture::TextureInfo info;
            info.physical_address = face.address;
            info.height = info.width = config.width;
            info.format = config.format;
            info.SetDefaultStride();
            auto surface = GetTextureSurface(info);
            if (surface) {
                face.watcher = surface->CreateWatcher();
            } else {
                // Can occur when texture address is invalid. We mark the watcher with nullptr in
                // this case and the content of the face wouldn't get updated. These are usually
                // leftover setup in the texture unit and games are not supposed to draw using them.
                face.watcher = nullptr;
            }
        }
    }

    if (cube.texture.handle == 0) {
        for (const Face& face : faces) {
            if (face.watcher) {
                auto surface = face.watcher->Get();
                cube.res_scale = std::max(cube.res_scale, surface->res_scale);
            }
        }

        cube.texture.Create();
        AllocateTextureCube(
            cube.texture.handle,
            GetFormatTuple(CachedSurface::PixelFormatFromTextureFormat(config.format)),
            cube.res_scale * config.width);
    }

    u32 scaled_size = cube.res_scale * config.width;

    OpenGLState prev_state = OpenGLState::GetCurState();
    SCOPE_EXIT({ prev_state.Apply(); });

    OpenGLState state;
    state.draw.read_framebuffer = read_framebuffer.handle;
    state.draw.draw_framebuffer = draw_framebuffer.handle;
    state.ResetTexture(cube.texture.handle);

    for (const Face& face : faces) {
        if (face.watcher && !face.watcher->IsValid()) {
            auto surface = face.watcher->Get();
            state.ResetTexture(surface->texture.handle);
            state.Apply();
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D,
                                   surface->texture.handle, 0);
            glFramebufferTexture2D(GL_READ_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                   0, 0);

            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, face.gl_face,
                                   cube.texture.handle, 0);
            glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_TEXTURE_2D,
                                   0, 0);

            auto src_rect = surface->GetScaledRect();
            glBlitFramebuffer(src_rect.left, src_rect.bottom, src_rect.right, src_rect.top, 0, 0,
                              scaled_size, scaled_size, GL_COLOR_BUFFER_BIT, GL_LINEAR);
            face.watcher->Validate();
        }
    }

    return cube;
}

SurfaceSurfaceRect_Tuple RasterizerCacheOpenGL::GetFramebufferSurfaces(
    bool using_color_fb, bool using_depth_fb, const MathUtil::Rectangle<s32>& viewport_rect) {
    const auto& regs = Pica::g_state.regs;
    const auto& config = regs.framebuffer.framebuffer;

    // update resolution_scale_factor and reset cache if changed
    static u16 resolution_scale_factor = GetResolutionScaleFactor();
    if (resolution_scale_factor != GetResolutionScaleFactor()) {
        resolution_scale_factor = GetResolutionScaleFactor();
        FlushAll();
        while (!surface_cache.empty())
            UnregisterSurface(*surface_cache.begin()->second.begin());
        texture_cube_cache.clear();
    }

    MathUtil::Rectangle<u32> viewport_clamped{
        static_cast<u32>(std::clamp(viewport_rect.left, 0, static_cast<s32>(config.GetWidth()))),
        static_cast<u32>(std::clamp(viewport_rect.top, 0, static_cast<s32>(config.GetHeight()))),
        static_cast<u32>(std::clamp(viewport_rect.right, 0, static_cast<s32>(config.GetWidth()))),
        static_cast<u32>(
            std::clamp(viewport_rect.bottom, 0, static_cast<s32>(config.GetHeight())))};

    // get color and depth surfaces
    SurfaceParams color_params;
    color_params.is_tiled = true;
    color_params.res_scale = resolution_scale_factor;
    color_params.width = config.GetWidth();
    color_params.height = config.GetHeight();
    SurfaceParams depth_params = color_params;

    color_params.addr = config.GetColorBufferPhysicalAddress();
    color_params.pixel_format = SurfaceParams::PixelFormatFromColorFormat(config.color_format);
    color_params.UpdateParams();

    depth_params.addr = config.GetDepthBufferPhysicalAddress();
    depth_params.pixel_format = SurfaceParams::PixelFormatFromDepthFormat(config.depth_format);
    depth_params.UpdateParams();

    auto color_vp_interval = color_params.GetSubRectInterval(viewport_clamped);
    auto depth_vp_interval = depth_params.GetSubRectInterval(viewport_clamped);

    // Make sure that framebuffers don't overlap if both color and depth are being used
    if (using_color_fb && using_depth_fb &&
        boost::icl::length(color_vp_interval & depth_vp_interval)) {
        LOG_CRITICAL(Render_OpenGL, "Color and depth framebuffer memory regions overlap; "
                                    "overlapping framebuffers not supported!");
        using_depth_fb = false;
    }

    MathUtil::Rectangle<u32> color_rect{};
    Surface color_surface = nullptr;
    if (using_color_fb)
        std::tie(color_surface, color_rect) =
            GetSurfaceSubRect(color_params, ScaleMatch::Exact, false);

    MathUtil::Rectangle<u32> depth_rect{};
    Surface depth_surface = nullptr;
    if (using_depth_fb)
        std::tie(depth_surface, depth_rect) =
            GetSurfaceSubRect(depth_params, ScaleMatch::Exact, false);

    MathUtil::Rectangle<u32> fb_rect{};
    if (color_surface != nullptr && depth_surface != nullptr) {
        fb_rect = color_rect;
        // Color and Depth surfaces must have the same dimensions and offsets
        if (color_rect.bottom != depth_rect.bottom || color_rect.top != depth_rect.top ||
            color_rect.left != depth_rect.left || color_rect.right != depth_rect.right) {
            color_surface = GetSurface(color_params, ScaleMatch::Exact, false);
            depth_surface = GetSurface(depth_params, ScaleMatch::Exact, false);
            fb_rect = color_surface->GetScaledRect();
        }
    } else if (color_surface != nullptr) {
        fb_rect = color_rect;
    } else if (depth_surface != nullptr) {
        fb_rect = depth_rect;
    }

    if (color_surface != nullptr) {
        ValidateSurface(color_surface, boost::icl::first(color_vp_interval),
                        boost::icl::length(color_vp_interval));
        color_surface->InvalidateAllWatcher();
    }
    if (depth_surface != nullptr) {
        ValidateSurface(depth_surface, boost::icl::first(depth_vp_interval),
                        boost::icl::length(depth_vp_interval));
        depth_surface->InvalidateAllWatcher();
    }

    return std::make_tuple(color_surface, depth_surface, fb_rect);
}

Surface RasterizerCacheOpenGL::GetFillSurface(const GPU::Regs::MemoryFillConfig& config) {
    Surface new_surface = std::make_shared<CachedSurface>();

    new_surface->addr = config.GetStartAddress();
    new_surface->end = config.GetEndAddress();
    new_surface->size = new_surface->end - new_surface->addr;
    new_surface->type = SurfaceType::Fill;
    new_surface->res_scale = std::numeric_limits<u16>::max();

    std::memcpy(&new_surface->fill_data[0], &config.value_32bit, 4);
    if (config.fill_32bit) {
        new_surface->fill_size = 4;
    } else if (config.fill_24bit) {
        new_surface->fill_size = 3;
    } else {
        new_surface->fill_size = 2;
    }

    RegisterSurface(new_surface);
    return new_surface;
}

SurfaceRect_Tuple RasterizerCacheOpenGL::GetTexCopySurface(const SurfaceParams& params) {
    MathUtil::Rectangle<u32> rect{};

    Surface match_surface = FindMatch<MatchFlags::TexCopy | MatchFlags::Invalid>(
        surface_cache, params, ScaleMatch::Ignore);

    if (match_surface != nullptr) {
        ValidateSurface(match_surface, params.addr, params.size);

        SurfaceParams match_subrect;
        if (params.width != params.stride) {
            const u32 tiled_size = match_surface->is_tiled ? 8 : 1;
            match_subrect = params;
            match_subrect.width = match_surface->PixelsInBytes(params.width) / tiled_size;
            match_subrect.stride = match_surface->PixelsInBytes(params.stride) / tiled_size;
            match_subrect.height *= tiled_size;
        } else {
            match_subrect = match_surface->FromInterval(params.GetInterval());
            ASSERT(match_subrect.GetInterval() == params.GetInterval());
        }

        rect = match_surface->GetScaledSubRect(match_subrect);
    }

    return std::make_tuple(match_surface, rect);
}

void RasterizerCacheOpenGL::DuplicateSurface(const Surface& src_surface,
                                             const Surface& dest_surface) {
    ASSERT(dest_surface->addr <= src_surface->addr && dest_surface->end >= src_surface->end);

    BlitSurfaces(src_surface, src_surface->GetScaledRect(), dest_surface,
                 dest_surface->GetScaledSubRect(*src_surface));

    dest_surface->invalid_regions -= src_surface->GetInterval();
    dest_surface->invalid_regions += src_surface->invalid_regions;

    SurfaceRegions regions;
    for (auto& pair : RangeFromInterval(dirty_regions, src_surface->GetInterval())) {
        if (pair.second == src_surface) {
            regions += pair.first;
        }
    }
    for (auto& interval : regions) {
        dirty_regions.set({interval, dest_surface});
    }
}

void RasterizerCacheOpenGL::ValidateSurface(const Surface& surface, PAddr addr, u32 size) {
    if (size == 0)
        return;

    const SurfaceInterval validate_interval(addr, addr + size);

    if (surface->type == SurfaceType::Fill) {
        // Sanity check, fill surfaces will always be valid when used
        ASSERT(surface->IsRegionValid(validate_interval));
        return;
    }

    while (true) {
        const auto it = surface->invalid_regions.find(validate_interval);
        if (it == surface->invalid_regions.end())
            break;

        const auto interval = *it & validate_interval;
        // Look for a valid surface to copy from
        SurfaceParams params = surface->FromInterval(interval);

        Surface copy_surface =
            FindMatch<MatchFlags::Copy>(surface_cache, params, ScaleMatch::Ignore, interval);
        if (copy_surface != nullptr) {
            SurfaceInterval copy_interval = params.GetCopyableInterval(copy_surface);
            CopySurface(copy_surface, surface, copy_interval);
            surface->invalid_regions.erase(copy_interval);
            continue;
        }

        // D24S8 to RGBA8
        if (surface->pixel_format == PixelFormat::RGBA8) {
            params.pixel_format = PixelFormat::D24S8;
            Surface reinterpret_surface =
                FindMatch<MatchFlags::Copy>(surface_cache, params, ScaleMatch::Ignore, interval);
            if (reinterpret_surface != nullptr) {
                ASSERT(reinterpret_surface->pixel_format == PixelFormat::D24S8);

                SurfaceInterval convert_interval = params.GetCopyableInterval(reinterpret_surface);
                SurfaceParams convert_params = surface->FromInterval(convert_interval);
                auto src_rect = reinterpret_surface->GetScaledSubRect(convert_params);
                auto dest_rect = surface->GetScaledSubRect(convert_params);

                ConvertD24S8toABGR(reinterpret_surface->texture.handle, src_rect,
                                   surface->texture.handle, dest_rect);

                surface->invalid_regions.erase(convert_interval);
                continue;
            }
        }

        // Load data from 3DS memory
        FlushRegion(params.addr, params.size);
        surface->LoadGLBuffer(params.addr, params.end);
        surface->UploadGLTexture(surface->GetSubRect(params), read_framebuffer.handle,
                                 draw_framebuffer.handle);
        surface->invalid_regions.erase(params.GetInterval());
    }
}

void RasterizerCacheOpenGL::FlushRegion(PAddr addr, u32 size, Surface flush_surface) {
    if (size == 0)
        return;

    const SurfaceInterval flush_interval(addr, addr + size);
    SurfaceRegions flushed_intervals;

    for (auto& pair : RangeFromInterval(dirty_regions, flush_interval)) {
        // small sizes imply that this most likely comes from the cpu, flush the entire region
        // the point is to avoid thousands of small writes every frame if the cpu decides to access
        // that region, anything higher than 8 you're guaranteed it comes from a service
        const auto interval = size <= 8 ? pair.first : pair.first & flush_interval;
        auto& surface = pair.second;

        if (flush_surface != nullptr && surface != flush_surface)
            continue;

        // Sanity check, this surface is the last one that marked this region dirty
        ASSERT(surface->IsRegionValid(interval));

        if (surface->type != SurfaceType::Fill) {
            SurfaceParams params = surface->FromInterval(interval);
            surface->DownloadGLTexture(surface->GetSubRect(params), read_framebuffer.handle,
                                       draw_framebuffer.handle);
        }
        surface->FlushGLBuffer(boost::icl::first(interval), boost::icl::last_next(interval));
        flushed_intervals += interval;
    }
    // Reset dirty regions
    dirty_regions -= flushed_intervals;
}

void RasterizerCacheOpenGL::FlushAll() {
    FlushRegion(0, 0xFFFFFFFF);
}

void RasterizerCacheOpenGL::InvalidateRegion(PAddr addr, u32 size, const Surface& region_owner) {
    if (size == 0)
        return;

    const SurfaceInterval invalid_interval(addr, addr + size);

    if (region_owner != nullptr) {
        ASSERT(region_owner->type != SurfaceType::Texture);
        ASSERT(addr >= region_owner->addr && addr + size <= region_owner->end);
        // Surfaces can't have a gap
        ASSERT(region_owner->width == region_owner->stride);
        region_owner->invalid_regions.erase(invalid_interval);
    }

    for (auto& pair : RangeFromInterval(surface_cache, invalid_interval)) {
        for (auto& cached_surface : pair.second) {
            if (cached_surface == region_owner)
                continue;

            // If cpu is invalidating this region we want to remove it
            // to (likely) mark the memory pages as uncached
            if (region_owner == nullptr && size <= 8) {
                FlushRegion(cached_surface->addr, cached_surface->size, cached_surface);
                remove_surfaces.emplace(cached_surface);
                continue;
            }

            const auto interval = cached_surface->GetInterval() & invalid_interval;
            cached_surface->invalid_regions.insert(interval);

            // Remove only "empty" fill surfaces to avoid destroying and recreating OGL textures
            if (cached_surface->type == SurfaceType::Fill &&
                cached_surface->IsSurfaceFullyInvalid()) {
                remove_surfaces.emplace(cached_surface);
            }
        }
    }

    if (region_owner != nullptr)
        dirty_regions.set({invalid_interval, region_owner});
    else
        dirty_regions.erase(invalid_interval);

    for (auto& remove_surface : remove_surfaces) {
        if (remove_surface == region_owner) {
            Surface expanded_surface = FindMatch<MatchFlags::SubRect | MatchFlags::Invalid>(
                surface_cache, *region_owner, ScaleMatch::Ignore);
            ASSERT(expanded_surface);

            if ((region_owner->invalid_regions - expanded_surface->invalid_regions).empty()) {
                DuplicateSurface(region_owner, expanded_surface);
            } else {
                continue;
            }
        }
        UnregisterSurface(remove_surface);
    }

    remove_surfaces.clear();
}

Surface RasterizerCacheOpenGL::CreateSurface(const SurfaceParams& params) {
    Surface surface = std::make_shared<CachedSurface>();
    static_cast<SurfaceParams&>(*surface) = params;

    surface->texture.Create();

    surface->gl_buffer_size = 0;
    surface->invalid_regions.insert(surface->GetInterval());
    AllocateSurfaceTexture(surface->texture.handle, GetFormatTuple(surface->pixel_format),
                           surface->GetScaledWidth(), surface->GetScaledHeight());

    return surface;
}

void RasterizerCacheOpenGL::RegisterSurface(const Surface& surface) {
    if (surface->registered) {
        return;
    }
    surface->registered = true;
    surface_cache.add({surface->GetInterval(), SurfaceSet{surface}});
    UpdatePagesCachedCount(surface->addr, surface->size, 1);
}

void RasterizerCacheOpenGL::UnregisterSurface(const Surface& surface) {
    if (!surface->registered) {
        return;
    }
    surface->registered = false;
    UpdatePagesCachedCount(surface->addr, surface->size, -1);
    surface_cache.subtract({surface->GetInterval(), SurfaceSet{surface}});
}

void RasterizerCacheOpenGL::UpdatePagesCachedCount(PAddr addr, u32 size, int delta) {
    const u32 num_pages =
        ((addr + size - 1) >> Memory::PAGE_BITS) - (addr >> Memory::PAGE_BITS) + 1;
    const u32 page_start = addr >> Memory::PAGE_BITS;
    const u32 page_end = page_start + num_pages;

    // Interval maps will erase segments if count reaches 0, so if delta is negative we have to
    // subtract after iterating
    const auto pages_interval = PageMap::interval_type::right_open(page_start, page_end);
    if (delta > 0)
        cached_pages.add({pages_interval, delta});

    for (auto& pair : RangeFromInterval(cached_pages, pages_interval)) {
        const auto interval = pair.first & pages_interval;
        const int count = pair.second;

        const PAddr interval_start_addr = boost::icl::first(interval) << Memory::PAGE_BITS;
        const PAddr interval_end_addr = boost::icl::last_next(interval) << Memory::PAGE_BITS;
        const u32 interval_size = interval_end_addr - interval_start_addr;

        if (delta > 0 && count == delta)
            Memory::RasterizerMarkRegionCached(interval_start_addr, interval_size, true);
        else if (delta < 0 && count == -delta)
            Memory::RasterizerMarkRegionCached(interval_start_addr, interval_size, false);
        else
            ASSERT(count >= 0);
    }

    if (delta < 0)
        cached_pages.add({pages_interval, delta});
}
