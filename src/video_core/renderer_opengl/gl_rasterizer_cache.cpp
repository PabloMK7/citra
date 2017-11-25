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

using SurfaceType = SurfaceParams::SurfaceType;
using PixelFormat = SurfaceParams::PixelFormat;

static std::array<OGLFramebuffer, 2> transfer_framebuffers;

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

static constexpr FormatTuple tex_tuple = {GL_RGBA8, GL_RGBA, GL_UNSIGNED_BYTE};

static const FormatTuple& GetFormatTuple(PixelFormat pixel_format) {
    const SurfaceType type = SurfaceParams::GetFormatType(pixel_format);
    if (type == SurfaceType::Color) {
        ASSERT((size_t)pixel_format < fb_format_tuples.size());
        return fb_format_tuples[(unsigned int)pixel_format];
    } else if (type == SurfaceType::Depth || type == SurfaceType::DepthStencil) {
        size_t tuple_idx = (size_t)pixel_format - 14;
        ASSERT(tuple_idx < depth_format_tuples.size());
        return depth_format_tuples[tuple_idx];
    } else {
        return tex_tuple;
    }
}

template <typename Map, typename Interval>
constexpr auto RangeFromInterval(Map& map, const Interval& interval) {
    return boost::make_iterator_range(map.equal_range(interval));
}

static bool FillSurface(const Surface& surface, const u8* fill_data,
                        const MathUtil::Rectangle<u32>& fill_rect) {
    OpenGLState state = OpenGLState::GetCurState();

    OpenGLState prev_state = state;
    SCOPE_EXIT({ prev_state.Apply(); });

    state.ResetTexture(surface->texture.handle);

    state.scissor.enabled = true;
    state.scissor.x = static_cast<GLint>(fill_rect.left);
    state.scissor.y = static_cast<GLint>(fill_rect.bottom);
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

static bool BlitTextures(GLuint src_tex, const MathUtil::Rectangle<u32>& src_rect, GLuint dst_tex,
                         const MathUtil::Rectangle<u32>& dst_rect, SurfaceType type) {
    OpenGLState state = OpenGLState::GetCurState();

    OpenGLState prev_state = state;
    SCOPE_EXIT({ prev_state.Apply(); });

    // Make sure textures aren't bound to texture units, since going to bind them to framebuffer
    // components
    state.ResetTexture(src_tex);
    state.ResetTexture(dst_tex);

    // Keep track of previous framebuffer bindings
    state.draw.read_framebuffer = transfer_framebuffers[0].handle;
    state.draw.draw_framebuffer = transfer_framebuffers[1].handle;
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

    glBlitFramebuffer(src_rect.left, src_rect.bottom, src_rect.right, src_rect.top, dst_rect.left,
                      dst_rect.bottom, dst_rect.right, dst_rect.top, buffers,
                      buffers == GL_COLOR_BUFFER_BIT ? GL_LINEAR : GL_NEAREST);

    return true;
}

SurfaceParams SurfaceParams::FromInterval(SurfaceInterval interval) const {
    SurfaceParams params = *this;

    const u32 stride_tiled_bytes = BytesInPixels(stride * (is_tiled ? 8 : 1));
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
        params.width = PixelsInBytes(aligned_end - aligned_start) / (is_tiled ? 8 : 1);
        params.height = is_tiled ? 8 : 1;
    }
    params.UpdateParams();

    return params;
}

SurfaceInterval SurfaceParams::GetSubRectInterval(MathUtil::Rectangle<u32> unscaled_rect) const {
    if (unscaled_rect.GetHeight() == 0 || unscaled_rect.GetWidth() == 0) {
        return {};
    }

    if (unscaled_rect.bottom > unscaled_rect.top) {
        std::swap(unscaled_rect.top, unscaled_rect.bottom);
    }

    if (is_tiled) {
        unscaled_rect.left = Common::AlignDown(unscaled_rect.left, 8) * 8;
        unscaled_rect.bottom = Common::AlignDown(unscaled_rect.bottom, 8) / 8;
        unscaled_rect.right = Common::AlignUp(unscaled_rect.right, 8) * 8;
        unscaled_rect.top = Common::AlignUp(unscaled_rect.top, 8) / 8;
    }

    const u32 stride_tiled = (!is_tiled ? stride : stride * 8);

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
        return MathUtil::Rectangle<u32>(x0, height - y0, x0 + sub_surface.width,
                                        height - (y0 + sub_surface.height)); // Top to bottom
    }

    const int x0 = begin_pixel_index % stride;
    const int y0 = begin_pixel_index / stride;
    return MathUtil::Rectangle<u32>(x0, y0 + sub_surface.height, x0 + sub_surface.width,
                                    y0); // Bottom to top
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
    return (other_surface.addr == addr && other_surface.width == width &&
            other_surface.height == height && other_surface.stride == stride &&
            other_surface.pixel_format == pixel_format && other_surface.is_tiled == is_tiled);
}

bool SurfaceParams::CanSubRect(const SurfaceParams& sub_surface) const {
    if (sub_surface.addr < addr || sub_surface.end > end || sub_surface.stride != stride ||
        sub_surface.pixel_format != pixel_format || sub_surface.is_tiled != is_tiled ||
        (sub_surface.addr - addr) * 8 % GetFormatBpp() != 0)
        return false;

    auto rect = GetSubRect(sub_surface);

    if (rect.left + sub_surface.width > stride) {
        return false;
    }

    if (is_tiled) {
        return PixelsInBytes(sub_surface.addr - addr) % 64 == 0 && sub_surface.height % 8 == 0 &&
               sub_surface.width % 8 == 0;
    }

    return true;
}

bool SurfaceParams::CanExpand(const SurfaceParams& expanded_surface) const {
    if (pixel_format == PixelFormat::Invalid || pixel_format != expanded_surface.pixel_format ||
        is_tiled != expanded_surface.is_tiled || addr > expanded_surface.end ||
        expanded_surface.addr > end || stride != expanded_surface.stride)
        return false;

    const u32 byte_offset =
        std::max(expanded_surface.addr, addr) - std::min(expanded_surface.addr, addr);

    const int x0 = byte_offset % BytesInPixels(stride);
    const int y0 = byte_offset / BytesInPixels(stride);

    return x0 == 0 && (!is_tiled || y0 % 8 == 0);
}

bool SurfaceParams::CanTexCopy(const SurfaceParams& texcopy_params) const {
    if (pixel_format == PixelFormat::Invalid || addr > texcopy_params.addr ||
        end < texcopy_params.end || ((texcopy_params.addr - addr) * 8) % GetFormatBpp() != 0 ||
        (texcopy_params.width * 8) % GetFormatBpp() != 0 ||
        (texcopy_params.stride * 8) % GetFormatBpp() != 0)
        return false;

    const u32 begin_pixel_index = PixelsInBytes(texcopy_params.addr - addr);
    const int x0 = begin_pixel_index % stride;
    const int y0 = begin_pixel_index / stride;

    if (!is_tiled)
        return ((texcopy_params.height == 1 || PixelsInBytes(texcopy_params.stride) == stride) &&
                x0 + PixelsInBytes(texcopy_params.width) <= stride);

    return (PixelsInBytes(texcopy_params.addr - addr) % 64 == 0 &&
            PixelsInBytes(texcopy_params.width) % 64 == 0 &&
            (texcopy_params.height == 1 || PixelsInBytes(texcopy_params.stride) == stride * 8) &&
            x0 + PixelsInBytes(texcopy_params.width / 8) <= stride);
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

            const auto load_interval = SurfaceInterval(load_start, load_end);
            const auto rect = GetSubRect(FromInterval(load_interval));
            ASSERT(FromInterval(load_interval).GetInterval() == load_interval);

            for (unsigned y = rect.bottom; y < rect.top; ++y) {
                for (unsigned x = rect.left; x < rect.right; ++x) {
                    auto vec4 =
                        Pica::Texture::LookupTexture(texture_src_data, x, height - 1 - y, tex_info);
                    const size_t offset = (x + (width * y)) * 4;
                    std::memcpy(&gl_buffer[offset], vec4.AsArray(), 4);
                }
            }
        } else {
            morton_to_gl_fns[static_cast<size_t>(pixel_format)](stride, height, &gl_buffer[0], addr,
                                                                load_start, load_end);
        }
    }
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

        for (u32 offset = coarse_start_offset; offset < end_offset; offset += fill_size)
            std::memcpy(&dst_buffer[offset], &fill_data[0],
                        std::min(fill_size, end_offset - offset));

        if (backup_bytes)
            std::memcpy(&dst_buffer[coarse_start_offset], &backup_data[0], backup_bytes);
    } else if (!is_tiled) {
        ASSERT(type == SurfaceType::Color);
        std::memcpy(dst_buffer + start_offset, &gl_buffer[start_offset], flush_end - flush_start);
    } else {
        gl_to_morton_fns[static_cast<size_t>(pixel_format)](stride, height, &gl_buffer[0], addr,
                                                            flush_start, flush_end);
    }
}

void CachedSurface::UploadGLTexture(const MathUtil::Rectangle<u32>& rect) {
    if (type == SurfaceType::Fill)
        return;

    ASSERT(gl_buffer_size == width * height * GetGLBytesPerPixel(pixel_format));

    // Load data from memory to the surface
    GLint x0 = static_cast<GLint>(rect.left);
    GLint y0 = static_cast<GLint>(rect.bottom);
    size_t buffer_offset = (y0 * stride + x0) * GetGLBytesPerPixel(pixel_format);

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
                     scaled_rect, type);
    }
}
void CachedSurface::DownloadGLTexture(const MathUtil::Rectangle<u32>& rect) {
    if (type == SurfaceType::Fill)
        return;

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
    size_t buffer_offset = (rect.bottom * stride + rect.left) * GetGLBytesPerPixel(pixel_format);

    // If not 1x scale, blit scaled texture to a new 1x texture and use that to flush
    OGLTexture unscaled_tex;
    if (res_scale != 1) {
        auto scaled_rect = rect;
        scaled_rect.left *= res_scale;
        scaled_rect.top *= res_scale;
        scaled_rect.right *= res_scale;
        scaled_rect.bottom *= res_scale;

        unscaled_tex.Create();
        AllocateSurfaceTexture(unscaled_tex.handle, tuple, rect.GetWidth(), rect.GetHeight());
        BlitTextures(texture.handle, scaled_rect, unscaled_tex.handle, rect, type);

        state.texture_units[0].texture_2d = unscaled_tex.handle;
        state.Apply();

        glActiveTexture(GL_TEXTURE0);
        glGetTexImage(GL_TEXTURE_2D, 0, tuple.format, tuple.type, &gl_buffer[buffer_offset]);
    } else {
        state.ResetTexture(texture.handle);
        state.draw.read_framebuffer = transfer_framebuffers[0].handle;
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
                  boost::optional<SurfaceInterval> validate_interval = boost::none) {
    Surface match_surface = nullptr;
    bool match_valid = false;
    u32 match_scale = 0;
    SurfaceInterval match_interval{};

    for (auto& pair : RangeFromInterval(surface_cache, params.GetInterval())) {
        for (auto& surface : pair.second) {
            const bool res_scale_matched = match_scale_type == ScaleMatch::Exact
                                               ? (params.res_scale == surface->res_scale)
                                               : (params.res_scale <= surface->res_scale);
            bool is_valid =
                find_flags & MatchFlags::Copy ? true
                                              : // validity will be checked in GetCopyableInterval
                    surface->IsRegionValid(validate_interval.value_or(params.GetInterval()));

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

void RasterizerCacheOpenGL::FlushRegion(PAddr addr, u32 size, Surface flush_surface) {
    if (size == 0)
        return;

    const auto flush_interval = SurfaceInterval(addr, addr + size);
    for (auto& pair : RangeFromInterval(dirty_regions, flush_interval)) {
        const auto interval = pair.first & flush_interval;
        auto& surface = pair.second;

        if (flush_surface != nullptr && surface != flush_surface)
            continue;

        // Sanity check, this surface is the last one that marked this region dirty
        ASSERT(surface->IsRegionValid(interval));

        if (surface->type != SurfaceType::Fill) {
            SurfaceParams params = surface->FromInterval(interval);
            surface->DownloadGLTexture(surface->GetSubRect(params));
        }
        surface->FlushGLBuffer(boost::icl::first(interval), boost::icl::last_next(interval));
    }

    // Reset dirty regions
    dirty_regions.erase(flush_interval);
}

void RasterizerCacheOpenGL::FlushAll() {
    FlushRegion(0, 0xFFFFFFFF);
}

void RasterizerCacheOpenGL::InvalidateRegion(PAddr addr, u32 size, const Surface& region_owner) {
    if (size == 0)
        return;

    const auto invalid_interval = SurfaceInterval(addr, addr + size);

    if (region_owner != nullptr) {
        ASSERT(region_owner->type != SurfaceType::Texture);
        ASSERT(addr >= region_owner->addr && addr + size <= region_owner->end);
        ASSERT(region_owner->width == region_owner->stride); // Surfaces can't have a gap
        region_owner->invalid_regions.erase(invalid_interval);
    }

    for (auto& pair : RangeFromInterval(surface_cache, invalid_interval)) {
        for (auto& cached_surface : pair.second) {
            if (cached_surface == region_owner)
                continue;

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
    surface_cache.add({surface->GetInterval(), SurfaceSet{surface}});
    UpdatePagesCachedCount(surface->addr, surface->size, 1);
}

void RasterizerCacheOpenGL::UnregisterSurface(const Surface& surface) {
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
