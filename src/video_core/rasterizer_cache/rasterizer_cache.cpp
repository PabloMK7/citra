// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <optional>
#include <boost/range/iterator_range.hpp>
#include "common/alignment.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/scope_exit.h"
#include "video_core/pica_state.h"
#include "video_core/rasterizer_cache/rasterizer_cache.h"
#include "video_core/renderer_opengl/texture_filters/texture_filterer.h"
#include "video_core/renderer_opengl/texture_downloader_es.h"
#include "video_core/renderer_opengl/gl_format_reinterpreter.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/gl_vars.h"

namespace OpenGL {

// TODO: Deduplicate this
static Aspect ToAspect(SurfaceType type) {
    switch (type) {
    case SurfaceType::Color:
    case SurfaceType::Texture:
    case SurfaceType::Fill:
        return Aspect::Color;
    case SurfaceType::Depth:
        return Aspect::Depth;
    case SurfaceType::DepthStencil:
        return Aspect::DepthStencil;
    default:
        LOG_CRITICAL(Render_OpenGL, "Unknown SurfaceType {}", type);
        UNREACHABLE();
    }

    return Aspect::Color;
}

static ClearValue ToClearValue(Aspect aspect, PixelFormat format, const u8* fill_data) {
    ClearValue result{};
    switch (aspect) {
    case Aspect::Color: {
        Pica::Texture::TextureInfo tex_info{};
        tex_info.format = static_cast<Pica::TexturingRegs::TextureFormat>(format);

        Common::Vec4<u8> color = Pica::Texture::LookupTexture(fill_data, 0, 0, tex_info);
        result.color = color / 255.f;
        break;
    }
    case Aspect::Depth: {
        u32 depth_uint = 0;
        if (format == PixelFormat::D16) {
            std::memcpy(&depth_uint, fill_data, 2);
            result.depth = depth_uint / 65535.0f; // 2^16 - 1
        } else if (format == PixelFormat::D24) {
            std::memcpy(&depth_uint, fill_data, 3);
            result.depth = depth_uint / 16777215.0f; // 2^24 - 1
        }
        break;
    }
    case Aspect::DepthStencil: {
        u32 clear_value_uint;
        std::memcpy(&clear_value_uint, fill_data, sizeof(u32));

        result.depth = (clear_value_uint & 0xFFFFFF) / 16777215.0f; // 2^24 - 1
        result.stencil = (clear_value_uint >> 24);
        break;
    }
    }

    return result;
}

template <typename Map, typename Interval>
static constexpr auto RangeFromInterval(Map& map, const Interval& interval) {
    return boost::make_iterator_range(map.equal_range(interval));
}

// Allocate an uninitialized texture of appropriate size and format for the surface
OGLTexture RasterizerCacheOpenGL::AllocateSurfaceTexture(const FormatTuple& format_tuple, u32 width,
                                                         u32 height) {
    auto recycled_tex = host_texture_recycler.find({format_tuple, width, height});
    if (recycled_tex != host_texture_recycler.end()) {
        OGLTexture texture = std::move(recycled_tex->second);
        host_texture_recycler.erase(recycled_tex);
        return texture;
    }
    OGLTexture texture;
    texture.Create();

    OpenGLState cur_state = OpenGLState::GetCurState();
    // Keep track of previous texture bindings
    GLuint old_tex = cur_state.texture_units[0].texture_2d;
    cur_state.texture_units[0].texture_2d = texture.handle;
    cur_state.Apply();
    glActiveTexture(GL_TEXTURE0);

    if (GL_ARB_texture_storage) {
        // Allocate all possible mipmap levels upfront
        const GLsizei levels = static_cast<GLsizei>(std::log2(std::max(width, height))) + 1;
        glTexStorage2D(GL_TEXTURE_2D, levels, format_tuple.internal_format, width, height);
    } else {
        glTexImage2D(GL_TEXTURE_2D, 0, format_tuple.internal_format, width, height, 0,
                     format_tuple.format, format_tuple.type, nullptr);
    }

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);

    // Restore previous texture bindings
    cur_state.texture_units[0].texture_2d = old_tex;
    cur_state.Apply();

    return texture;
}

static void AllocateTextureCube(GLuint texture, const FormatTuple& format_tuple, u32 width) {
    OpenGLState cur_state = OpenGLState::GetCurState();

    // Keep track of previous texture bindings
    GLuint old_tex = cur_state.texture_cube_unit.texture_cube;
    cur_state.texture_cube_unit.texture_cube = texture;
    cur_state.Apply();
    glActiveTexture(TextureUnits::TextureCube.Enum());
    if (GL_ARB_texture_storage) {
        // Allocate all possible mipmap levels in case the game uses them later
        const GLsizei levels = static_cast<GLsizei>(std::log2(width)) + 1;
        glTexStorage2D(GL_TEXTURE_CUBE_MAP, levels, format_tuple.internal_format, width, width);
    } else {
        for (auto faces : {
                 GL_TEXTURE_CUBE_MAP_POSITIVE_X,
                 GL_TEXTURE_CUBE_MAP_POSITIVE_Y,
                 GL_TEXTURE_CUBE_MAP_POSITIVE_Z,
                 GL_TEXTURE_CUBE_MAP_NEGATIVE_X,
                 GL_TEXTURE_CUBE_MAP_NEGATIVE_Y,
                 GL_TEXTURE_CUBE_MAP_NEGATIVE_Z,
             }) {
            glTexImage2D(faces, 0, format_tuple.internal_format, width, width, 0,
                         format_tuple.format, format_tuple.type, nullptr);
        }
    }

    // Restore previous texture bindings
    cur_state.texture_cube_unit.texture_cube = old_tex;
    cur_state.Apply();
}

MICROPROFILE_DEFINE(OpenGL_CopySurface, "OpenGL", "CopySurface", MP_RGB(128, 192, 64));
void RasterizerCacheOpenGL::CopySurface(const Surface& src_surface, const Surface& dst_surface,
                                        SurfaceInterval copy_interval) {
    MICROPROFILE_SCOPE(OpenGL_CopySurface);

    SurfaceParams subrect_params = dst_surface->FromInterval(copy_interval);
    ASSERT(subrect_params.GetInterval() == copy_interval);
    ASSERT(src_surface != dst_surface);

    // This is only called when CanCopy is true, no need to run checks here
    const Aspect aspect = ToAspect(dst_surface->type);
    if (src_surface->type == SurfaceType::Fill) {
        // FillSurface needs a 4 bytes buffer
        const u32 fill_offset =
            (boost::icl::first(copy_interval) - src_surface->addr) % src_surface->fill_size;
        std::array<u8, 4> fill_buffer;

        u32 fill_buff_pos = fill_offset;
        for (std::size_t i = 0; i < fill_buffer.size(); i++) {
            fill_buffer[i] = src_surface->fill_data[fill_buff_pos++ % src_surface->fill_size];
        }

        const auto clear_rect = dst_surface->GetScaledSubRect(subrect_params);
        const ClearValue clear_value = ToClearValue(aspect, dst_surface->pixel_format,
                                                    fill_buffer.data());

        runtime.ClearTexture(dst_surface->texture, {aspect, clear_rect}, clear_value);
        return;
    }

    if (src_surface->CanSubRect(subrect_params)) {
        const auto src_rect = src_surface->GetScaledSubRect(subrect_params);
        const auto dst_rect = dst_surface->GetScaledSubRect(subrect_params);

        runtime.BlitTextures(src_surface->texture, {aspect, src_rect},
                             dst_surface->texture, {aspect, dst_rect});
        return;
    }

    UNREACHABLE();
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

static constexpr MatchFlags operator|(MatchFlags lhs, MatchFlags rhs) {
    return static_cast<MatchFlags>(static_cast<int>(lhs) | static_cast<int>(rhs));
}

/// Get the best surface match (and its match type) for the given flags
template <MatchFlags find_flags>
static Surface FindMatch(const SurfaceCache& surface_cache, const SurfaceParams& params,
                         ScaleMatch match_scale_type,
                         std::optional<SurfaceInterval> validate_interval = std::nullopt) {
    Surface match_surface = nullptr;
    bool match_valid = false;
    u32 match_scale = 0;
    SurfaceInterval match_interval{};

    for (const auto& pair : RangeFromInterval(surface_cache, params.GetInterval())) {
        for (const auto& surface : pair.second) {
            const bool res_scale_matched = match_scale_type == ScaleMatch::Exact
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
    resolution_scale_factor = VideoCore::GetResolutionScaleFactor();
    texture_filterer = std::make_unique<TextureFilterer>(Settings::values.texture_filter_name,
                                                         resolution_scale_factor);
    format_reinterpreter = std::make_unique<FormatReinterpreterOpenGL>();
    if (GLES) {
        texture_downloader_es = std::make_unique<TextureDownloaderES>(false);
    }
}

RasterizerCacheOpenGL::~RasterizerCacheOpenGL() {
#ifndef ANDROID
    // This is for switching renderers, which is unsupported on Android, and costly on shutdown
    ClearAll(false);
#endif
}

MICROPROFILE_DEFINE(OpenGL_BlitSurface, "OpenGL", "BlitSurface", MP_RGB(128, 192, 64));
bool RasterizerCacheOpenGL::BlitSurfaces(const Surface& src_surface,
                                         const Common::Rectangle<u32>& src_rect,
                                         const Surface& dst_surface,
                                         const Common::Rectangle<u32>& dst_rect) {
    MICROPROFILE_SCOPE(OpenGL_BlitSurface);

    if (CheckFormatsBlittable(src_surface->pixel_format, dst_surface->pixel_format)) {
        dst_surface->InvalidateAllWatcher();

        const Aspect aspect = ToAspect(src_surface->type);
        return runtime.BlitTextures(src_surface->texture, {aspect, src_rect},
                                    dst_surface->texture, {aspect, dst_rect});
    }

    return false;
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
            // This surface may have a subrect of another surface with a higher res_scale, find
            // it to adjust our params
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
        return std::make_tuple(nullptr, Common::Rectangle<u32>{});
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
            surface->UnlinkAllWatcher(); // unlink watchers as if this surface is already deleted
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
    return GetTextureSurface(info, config.config.lod.max_level);
}

Surface RasterizerCacheOpenGL::GetTextureSurface(const Pica::Texture::TextureInfo& info,
                                                 u32 max_level) {
    if (info.physical_address == 0) {
        return nullptr;
    }

    SurfaceParams params;
    params.addr = info.physical_address;
    params.width = info.width;
    params.height = info.height;
    params.is_tiled = true;
    params.pixel_format = PixelFormatFromTextureFormat(info.format);
    params.res_scale = texture_filterer->IsNull() ? 1 : resolution_scale_factor;
    params.UpdateParams();

    u32 min_width = info.width >> max_level;
    u32 min_height = info.height >> max_level;
    if (min_width % 8 != 0 || min_height % 8 != 0) {
        LOG_CRITICAL(Render_OpenGL, "Texture size ({}x{}) is not multiple of 8", min_width,
                     min_height);
        return nullptr;
    }
    if (info.width != (min_width << max_level) || info.height != (min_height << max_level)) {
        LOG_CRITICAL(Render_OpenGL,
                     "Texture size ({}x{}) does not support required mipmap level ({})",
                     params.width, params.height, max_level);
        return nullptr;
    }

    auto surface = GetSurface(params, ScaleMatch::Ignore, true);
    if (!surface)
        return nullptr;

    // Update mipmap if necessary
    if (max_level != 0) {
        if (max_level >= 8) {
            // since PICA only supports texture size between 8 and 1024, there are at most eight
            // possible mipmap levels including the base.
            LOG_CRITICAL(Render_OpenGL, "Unsupported mipmap level {}", max_level);
            return nullptr;
        }

        // Allocate more mipmap level if necessary
        if (surface->max_level < max_level) {
            if (surface->is_custom || !texture_filterer->IsNull()) {
                // TODO: proper mipmap support for custom textures
                runtime.GenerateMipmaps(surface->texture, max_level);
            }

            surface->max_level = max_level;
        }

        // Blit mipmaps that have been invalidated
        SurfaceParams surface_params = *surface;
        for (u32 level = 1; level <= max_level; ++level) {
            // In PICA all mipmap levels are stored next to each other
            surface_params.addr +=
                surface_params.width * surface_params.height * surface_params.GetFormatBpp() / 8;
            surface_params.width /= 2;
            surface_params.height /= 2;
            surface_params.stride = 0; // reset stride and let UpdateParams re-initialize it
            surface_params.UpdateParams();

            auto& watcher = surface->level_watchers[level - 1];
            if (!watcher || !watcher->Get()) {
                auto level_surface = GetSurface(surface_params, ScaleMatch::Ignore, true);
                if (level_surface) {
                    watcher = level_surface->CreateWatcher();
                } else {
                    watcher = nullptr;
                }
            }

            if (watcher && !watcher->IsValid()) {
                auto level_surface = watcher->Get();
                if (!level_surface->invalid_regions.empty()) {
                    ValidateSurface(level_surface, level_surface->addr, level_surface->size);
                }

                if (!surface->is_custom && texture_filterer->IsNull()) {
                    const auto src_rect = level_surface->GetScaledRect();
                    const auto dst_rect = surface_params.GetScaledRect();
                    const Aspect aspect = ToAspect(surface->type);

                    runtime.BlitTextures(level_surface->texture, {aspect, src_rect},
                                         surface->texture, {aspect, dst_rect, level});
                }

                watcher->Validate();
            }
        }
    }

    return surface;
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
                // Can occur when texture address is invalid. We mark the watcher with nullptr
                // in this case and the content of the face wouldn't get updated. These are
                // usually leftover setup in the texture unit and games are not supposed to draw
                // using them.
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
            GetFormatTuple(PixelFormatFromTextureFormat(config.format)),
            cube.res_scale * config.width);
    }

    u32 scaled_size = cube.res_scale * config.width;

    OpenGLState prev_state = OpenGLState::GetCurState();
    SCOPE_EXIT({ prev_state.Apply(); });

    for (const Face& face : faces) {
        if (face.watcher && !face.watcher->IsValid()) {
            auto surface = face.watcher->Get();
            if (!surface->invalid_regions.empty()) {
                ValidateSurface(surface, surface->addr, surface->size);
            }

            const auto src_rect = surface->GetScaledRect();
            const auto dst_rect = Common::Rectangle<u32>{0, scaled_size, scaled_size, 0};
            const Aspect aspect = ToAspect(surface->type);
            runtime.BlitTextures(surface->texture, {aspect, src_rect},
                                 cube.texture, {aspect, dst_rect});

            face.watcher->Validate();
        }
    }

    return cube;
}

SurfaceSurfaceRect_Tuple RasterizerCacheOpenGL::GetFramebufferSurfaces(
    bool using_color_fb, bool using_depth_fb, const Common::Rectangle<s32>& viewport_rect) {
    const auto& regs = Pica::g_state.regs;
    const auto& config = regs.framebuffer.framebuffer;

    // update resolution_scale_factor and reset cache if changed
    if ((resolution_scale_factor != VideoCore::GetResolutionScaleFactor()) ||
        (VideoCore::g_texture_filter_update_requested.exchange(false) &&
         texture_filterer->Reset(Settings::values.texture_filter_name, resolution_scale_factor))) {
        resolution_scale_factor = VideoCore::GetResolutionScaleFactor();
        FlushAll();
        while (!surface_cache.empty())
            UnregisterSurface(*surface_cache.begin()->second.begin());
        texture_cube_cache.clear();
    }

    Common::Rectangle<u32> viewport_clamped{
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
    color_params.pixel_format = PixelFormatFromColorFormat(config.color_format);
    color_params.UpdateParams();

    depth_params.addr = config.GetDepthBufferPhysicalAddress();
    depth_params.pixel_format = PixelFormatFromDepthFormat(config.depth_format);
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

    Common::Rectangle<u32> color_rect{};
    Surface color_surface = nullptr;
    if (using_color_fb)
        std::tie(color_surface, color_rect) =
            GetSurfaceSubRect(color_params, ScaleMatch::Exact, false);

    Common::Rectangle<u32> depth_rect{};
    Surface depth_surface = nullptr;
    if (using_depth_fb)
        std::tie(depth_surface, depth_rect) =
            GetSurfaceSubRect(depth_params, ScaleMatch::Exact, false);

    Common::Rectangle<u32> fb_rect{};
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
    Surface new_surface = std::make_shared<CachedSurface>(*this, runtime);

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
    Common::Rectangle<u32> rect{};

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
    for (const auto& pair : RangeFromInterval(dirty_regions, src_surface->GetInterval())) {
        if (pair.second == src_surface) {
            regions += pair.first;
        }
    }
    for (const auto& interval : regions) {
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

    auto validate_regions = surface->invalid_regions & validate_interval;
    auto notify_validated = [&](SurfaceInterval interval) {
        surface->invalid_regions.erase(interval);
        validate_regions.erase(interval);
    };

    while (true) {
        const auto it = validate_regions.begin();
        if (it == validate_regions.end())
            break;

        const auto interval = *it & validate_interval;
        // Look for a valid surface to copy from
        SurfaceParams params = surface->FromInterval(interval);

        Surface copy_surface =
            FindMatch<MatchFlags::Copy>(surface_cache, params, ScaleMatch::Ignore, interval);
        if (copy_surface != nullptr) {
            SurfaceInterval copy_interval = params.GetCopyableInterval(copy_surface);
            CopySurface(copy_surface, surface, copy_interval);
            notify_validated(copy_interval);
            continue;
        }

        // Try to find surface in cache with different format
        // that can can be reinterpreted to the requested format.
        if (ValidateByReinterpretation(surface, params, interval)) {
            notify_validated(interval);
            continue;
        }
        // Could not find a matching reinterpreter, check if we need to implement a
        // reinterpreter
        if (NoUnimplementedReinterpretations(surface, params, interval) &&
            !IntervalHasInvalidPixelFormat(params, interval)) {
            // No surfaces were found in the cache that had a matching bit-width.
            // If the region was created entirely on the GPU,
            // assume it was a developer mistake and skip flushing.
            if (boost::icl::contains(dirty_regions, interval)) {
                LOG_DEBUG(Render_OpenGL, "Region created fully on GPU and reinterpretation is "
                                         "invalid. Skipping validation");
                validate_regions.erase(interval);
                continue;
            }
        }

        // Load data from 3DS memory
        FlushRegion(params.addr, params.size);
        surface->LoadGLBuffer(params.addr, params.end);
        surface->UploadGLTexture(surface->GetSubRect(params));
        notify_validated(params.GetInterval());
    }
}

bool RasterizerCacheOpenGL::NoUnimplementedReinterpretations(const Surface& surface,
                                                             SurfaceParams& params,
                                                             const SurfaceInterval& interval) {
    static constexpr std::array<PixelFormat, 17> all_formats{
        PixelFormat::RGBA8, PixelFormat::RGB8,   PixelFormat::RGB5A1, PixelFormat::RGB565,
        PixelFormat::RGBA4, PixelFormat::IA8,    PixelFormat::RG8,    PixelFormat::I8,
        PixelFormat::A8,    PixelFormat::IA4,    PixelFormat::I4,     PixelFormat::A4,
        PixelFormat::ETC1,  PixelFormat::ETC1A4, PixelFormat::D16,    PixelFormat::D24,
        PixelFormat::D24S8,
    };
    bool implemented = true;
    for (PixelFormat format : all_formats) {
        if (GetFormatBpp(format) == surface->GetFormatBpp()) {
            params.pixel_format = format;
            // This could potentially be expensive,
            // although experimentally it hasn't been too bad
            Surface test_surface =
                FindMatch<MatchFlags::Copy>(surface_cache, params, ScaleMatch::Ignore, interval);
            if (test_surface != nullptr) {
                LOG_WARNING(Render_OpenGL, "Missing pixel_format reinterpreter: {} -> {}",
                            PixelFormatAsString(format),
                            PixelFormatAsString(surface->pixel_format));
                implemented = false;
            }
        }
    }
    return implemented;
}

bool RasterizerCacheOpenGL::IntervalHasInvalidPixelFormat(SurfaceParams& params,
                                                          const SurfaceInterval& interval) {
    params.pixel_format = PixelFormat::Invalid;
    for (const auto& set : RangeFromInterval(surface_cache, interval))
        for (const auto& surface : set.second)
            if (surface->pixel_format == PixelFormat::Invalid && surface->type != SurfaceType::Fill) {
                LOG_WARNING(Render_OpenGL, "Surface {:#x} found with invalid pixel format", surface->addr);
                return true;
            }
    return false;
}

bool RasterizerCacheOpenGL::ValidateByReinterpretation(const Surface& surface,
                                                       SurfaceParams& params,
                                                       const SurfaceInterval& interval) {
    const PixelFormat dst_format = surface->pixel_format;
    const SurfaceType type = GetFormatType(dst_format);
    const FormatTuple& tuple = GetFormatTuple(dst_format);

    for (auto& reinterpreter :
         format_reinterpreter->GetPossibleReinterpretations(surface->pixel_format)) {

        params.pixel_format = reinterpreter->GetSourceFormat();
        Surface reinterpret_surface =
            FindMatch<MatchFlags::Copy>(surface_cache, params, ScaleMatch::Ignore, interval);

        if (reinterpret_surface != nullptr) {
            auto reinterpret_interval = params.GetCopyableInterval(reinterpret_surface);
            auto reinterpret_params = surface->FromInterval(reinterpret_interval);
            auto src_rect = reinterpret_surface->GetScaledSubRect(reinterpret_params);
            auto dest_rect = surface->GetScaledSubRect(reinterpret_params);

            if (!texture_filterer->IsNull() && reinterpret_surface->res_scale == 1 &&
                surface->res_scale == resolution_scale_factor) {
                // The destination surface is either a framebuffer, or a filtered texture.
                // Create an intermediate surface to convert to before blitting to the
                // destination.
                const u32 width = dest_rect.GetHeight() / resolution_scale_factor;
                const u32 height = dest_rect.GetWidth() / resolution_scale_factor;
                const Common::Rectangle<u32> tmp_rect{0, width, height, 0};

                OGLTexture tmp_tex = AllocateSurfaceTexture(tuple, height, width);
                reinterpreter->Reinterpret(reinterpret_surface->texture, src_rect,
                                            tmp_tex, tmp_rect);

                if (!texture_filterer->Filter(tmp_tex, tmp_rect,
                                              surface->texture, dest_rect, type)) {

                    const Aspect aspect = ToAspect(type);
                    runtime.BlitTextures(tmp_tex, {aspect, tmp_rect},
                                         surface->texture, {aspect, dest_rect});
                }
            } else {
                reinterpreter->Reinterpret(reinterpret_surface->texture, src_rect,
                                           surface->texture, dest_rect);
            }

            return true;
        }
    }

    return false;
}

void RasterizerCacheOpenGL::ClearAll(bool flush) {
    const auto flush_interval = PageMap::interval_type::right_open(0x0, 0xFFFFFFFF);
    // Force flush all surfaces from the cache
    if (flush) {
        FlushRegion(0x0, 0xFFFFFFFF);
    }
    // Unmark all of the marked pages
    for (auto& pair : RangeFromInterval(cached_pages, flush_interval)) {
        const auto interval = pair.first & flush_interval;

        const PAddr interval_start_addr = boost::icl::first(interval) << Memory::PAGE_BITS;
        const PAddr interval_end_addr = boost::icl::last_next(interval) << Memory::PAGE_BITS;
        const u32 interval_size = interval_end_addr - interval_start_addr;

        VideoCore::g_memory->RasterizerMarkRegionCached(interval_start_addr, interval_size, false);
    }

    // Remove the whole cache without really looking at it.
    cached_pages -= flush_interval;
    dirty_regions -= SurfaceInterval(0x0, 0xFFFFFFFF);
    surface_cache -= SurfaceInterval(0x0, 0xFFFFFFFF);
    remove_surfaces.clear();
}

void RasterizerCacheOpenGL::FlushRegion(PAddr addr, u32 size, Surface flush_surface) {
    std::lock_guard lock{mutex};

    if (size == 0)
        return;

    const SurfaceInterval flush_interval(addr, addr + size);
    SurfaceRegions flushed_intervals;

    for (auto& pair : RangeFromInterval(dirty_regions, flush_interval)) {
        // small sizes imply that this most likely comes from the cpu, flush the entire region
        // the point is to avoid thousands of small writes every frame if the cpu decides to
        // access that region, anything higher than 8 you're guaranteed it comes from a service
        const auto interval = size <= 8 ? pair.first : pair.first & flush_interval;
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
        flushed_intervals += interval;
    }
    // Reset dirty regions
    dirty_regions -= flushed_intervals;
}

void RasterizerCacheOpenGL::FlushAll() {
    FlushRegion(0, 0xFFFFFFFF);
}

void RasterizerCacheOpenGL::InvalidateRegion(PAddr addr, u32 size, const Surface& region_owner) {
    std::lock_guard lock{mutex};

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

    for (const auto& pair : RangeFromInterval(surface_cache, invalid_interval)) {
        for (const auto& cached_surface : pair.second) {
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
            cached_surface->InvalidateAllWatcher();

            // If the surface has no salvageable data it should be removed from the cache to avoid
            // clogging the data structure
            if (cached_surface->IsSurfaceFullyInvalid()) {
                remove_surfaces.emplace(cached_surface);
            }
        }
    }

    if (region_owner != nullptr)
        dirty_regions.set({invalid_interval, region_owner});
    else
        dirty_regions.erase(invalid_interval);

    for (const auto& remove_surface : remove_surfaces) {
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
    Surface surface = std::make_shared<CachedSurface>(*this, runtime);
    static_cast<SurfaceParams&>(*surface) = params;

    surface->invalid_regions.insert(surface->GetInterval());

    surface->texture =
        AllocateSurfaceTexture(GetFormatTuple(surface->pixel_format), surface->GetScaledWidth(),
                               surface->GetScaledHeight());

    return surface;
}

void RasterizerCacheOpenGL::RegisterSurface(const Surface& surface) {
    std::lock_guard lock{mutex};

    if (surface->registered) {
        return;
    }
    surface->registered = true;
    surface_cache.add({surface->GetInterval(), SurfaceSet{surface}});
    UpdatePagesCachedCount(surface->addr, surface->size, 1);
}

void RasterizerCacheOpenGL::UnregisterSurface(const Surface& surface) {
    std::lock_guard lock{mutex};

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

    for (const auto& pair : RangeFromInterval(cached_pages, pages_interval)) {
        const auto interval = pair.first & pages_interval;
        const int count = pair.second;

        const PAddr interval_start_addr = boost::icl::first(interval) << Memory::PAGE_BITS;
        const PAddr interval_end_addr = boost::icl::last_next(interval) << Memory::PAGE_BITS;
        const u32 interval_size = interval_end_addr - interval_start_addr;

        if (delta > 0 && count == delta)
            VideoCore::g_memory->RasterizerMarkRegionCached(interval_start_addr, interval_size,
                                                            true);
        else if (delta < 0 && count == -delta)
            VideoCore::g_memory->RasterizerMarkRegionCached(interval_start_addr, interval_size,
                                                            false);
        else
            ASSERT(count >= 0);
    }

    if (delta < 0)
        cached_pages.add({pages_interval, delta});
}

} // namespace OpenGL
