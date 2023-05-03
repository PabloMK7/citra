// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <optional>
#include <boost/range/iterator_range.hpp>
#include "common/alignment.h"
#include "common/logging/log.h"
#include "common/microprofile.h"
#include "common/settings.h"
#include "core/memory.h"
#include "video_core/custom_textures/custom_tex_manager.h"
#include "video_core/rasterizer_cache/rasterizer_cache.h"
#include "video_core/regs.h"
#include "video_core/renderer_base.h"
#include "video_core/renderer_opengl/gl_texture_runtime.h"

namespace VideoCore {

namespace {

MICROPROFILE_DEFINE(RasterizerCache_CopySurface, "RasterizerCache", "CopySurface",
                    MP_RGB(128, 192, 64));
MICROPROFILE_DEFINE(RasterizerCache_UploadSurface, "RasterizerCache", "UploadSurface",
                    MP_RGB(128, 192, 64));
MICROPROFILE_DEFINE(RasterizerCache_ComputeHash, "RasterizerCache", "ComputeHash",
                    MP_RGB(32, 64, 192));
MICROPROFILE_DEFINE(RasterizerCache_DownloadSurface, "RasterizerCache", "DownloadSurface",
                    MP_RGB(128, 192, 64));
MICROPROFILE_DEFINE(RasterizerCache_Invalidation, "RasterizerCache", "Invalidation",
                    MP_RGB(128, 64, 192));
MICROPROFILE_DEFINE(RasterizerCache_Flush, "RasterizerCache", "Flush", MP_RGB(128, 64, 192));

constexpr auto RangeFromInterval(const auto& map, const auto& interval) {
    return boost::make_iterator_range(map.equal_range(interval));
}

enum MatchFlags {
    Exact = 1 << 0,   ///< Surfaces perfectly match
    SubRect = 1 << 1, ///< Surface encompasses params
    Copy = 1 << 2,    ///< Surface we can copy from
    Expand = 1 << 3,  ///< Surface that can expand params
    TexCopy = 1 << 4, ///< Surface that will match a display transfer "texture copy" parameters
};

/// Get the best surface match (and its match type) for the given flags
template <MatchFlags find_flags>
auto FindMatch(const auto& surface_cache, const SurfaceParams& params, ScaleMatch match_scale_type,
               std::optional<SurfaceInterval> validate_interval = std::nullopt) {
    RasterizerCache::SurfaceRef match_surface = nullptr;
    bool match_valid = false;
    u32 match_scale = 0;
    SurfaceInterval match_interval{};

    for (const auto& pair : RangeFromInterval(surface_cache, params.GetInterval())) {
        for (const auto& surface : pair.second) {
            const bool res_scale_matched = match_scale_type == ScaleMatch::Exact
                                               ? (params.res_scale == surface->res_scale)
                                               : (params.res_scale <= surface->res_scale);
            // Validity will be checked in GetCopyableInterval
            const bool is_valid =
                find_flags & MatchFlags::Copy
                    ? true
                    : surface->IsRegionValid(validate_interval.value_or(params.GetInterval()));

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
                    surface->GetCopyableInterval(params.FromInterval(*validate_interval));
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

} // Anonymous namespace

RasterizerCache::RasterizerCache(Memory::MemorySystem& memory_,
                                 CustomTexManager& custom_tex_manager_,
                                 OpenGL::TextureRuntime& runtime_, Pica::Regs& regs_,
                                 RendererBase& renderer_)
    : memory{memory_}, custom_tex_manager{custom_tex_manager_}, runtime{runtime_}, regs{regs_},
      renderer{renderer_}, resolution_scale_factor{renderer.GetResolutionScaleFactor()},
      use_filter{Settings::values.texture_filter.GetValue() != Settings::TextureFilter::None},
      dump_textures{Settings::values.dump_textures.GetValue()},
      use_custom_textures{Settings::values.custom_textures.GetValue()} {}

RasterizerCache::~RasterizerCache() {
#ifndef ANDROID
    // This is for switching renderers, which is unsupported on Android, and costly on shutdown
    ClearAll(false);
#endif
}

void RasterizerCache::TickFrame() {
    custom_tex_manager.TickFrame();

    const u32 scale_factor = renderer.GetResolutionScaleFactor();
    const bool resolution_scale_changed = resolution_scale_factor != scale_factor;
    const bool use_custom_texture_changed =
        Settings::values.custom_textures.GetValue() != use_custom_textures;
    const bool texture_filter_changed =
        renderer.Settings().texture_filter_update_requested.exchange(false);

    if (resolution_scale_changed || texture_filter_changed || use_custom_texture_changed) {
        resolution_scale_factor = scale_factor;
        use_filter = Settings::values.texture_filter.GetValue() != Settings::TextureFilter::None;
        use_custom_textures = Settings::values.custom_textures.GetValue();
        if (use_custom_textures) {
            custom_tex_manager.FindCustomTextures();
        }
        FlushAll();
        while (!surface_cache.empty()) {
            UnregisterSurface(*surface_cache.begin()->second.begin());
        }
        texture_cube_cache.clear();
        runtime.Reset();
    }
}

bool RasterizerCache::AccelerateTextureCopy(const GPU::Regs::DisplayTransferConfig& config) {
    // Texture copy size is aligned to 16 byte units
    const u32 copy_size = Common::AlignDown(config.texture_copy.size, 16);
    if (copy_size == 0) {
        return false;
    }

    u32 input_gap = config.texture_copy.input_gap * 16;
    u32 input_width = config.texture_copy.input_width * 16;
    if (input_width == 0 && input_gap != 0) {
        return false;
    }
    if (input_gap == 0 || input_width >= copy_size) {
        input_width = copy_size;
        input_gap = 0;
    }
    if (copy_size % input_width != 0) {
        return false;
    }

    u32 output_gap = config.texture_copy.output_gap * 16;
    u32 output_width = config.texture_copy.output_width * 16;
    if (output_width == 0 && output_gap != 0) {
        return false;
    }
    if (output_gap == 0 || output_width >= copy_size) {
        output_width = copy_size;
        output_gap = 0;
    }
    if (copy_size % output_width != 0) {
        return false;
    }

    SurfaceParams src_params;
    src_params.addr = config.GetPhysicalInputAddress();
    src_params.stride = input_width + input_gap; // stride in bytes
    src_params.width = input_width;              // width in bytes
    src_params.height = copy_size / input_width;
    src_params.size = ((src_params.height - 1) * src_params.stride) + src_params.width;
    src_params.end = src_params.addr + src_params.size;

    const auto [src_surface, src_rect] = GetTexCopySurface(src_params);
    if (!src_surface) {
        return false;
    }

    // If the output gap is nonzero ensure the output width matches the source rectangle width,
    // otherwise we cannot use hardware accelerated texture copy. The former is in terms of bytes
    // not pixels so first get the unscaled copy width and calculate the bytes this corresponds to.
    // Note that tiled textures are laid out sequentially in memory, so we multiply that by eight
    // to get the correct byte count.
    if (output_gap != 0 &&
        (output_width != src_surface->BytesInPixels(src_rect.GetWidth() / src_surface->res_scale) *
                             (src_surface->is_tiled ? 8 : 1) ||
         output_gap % src_surface->BytesInPixels(src_surface->is_tiled ? 64 : 1) != 0)) {
        return false;
    }

    SurfaceParams dst_params = *src_surface;
    dst_params.addr = config.GetPhysicalOutputAddress();
    dst_params.width = src_rect.GetWidth() / src_surface->res_scale;
    dst_params.stride = dst_params.width + src_surface->PixelsInBytes(
                                               src_surface->is_tiled ? output_gap / 8 : output_gap);
    dst_params.height = src_rect.GetHeight() / src_surface->res_scale;
    dst_params.res_scale = src_surface->res_scale;
    dst_params.UpdateParams();

    // Since we are going to invalidate the gap if there is one, we will have to load it first
    const bool load_gap = output_gap != 0;
    const auto [dst_surface, dst_rect] =
        GetSurfaceSubRect(dst_params, ScaleMatch::Upscale, load_gap);

    if (!dst_surface || dst_surface->type == SurfaceType::Texture ||
        !CheckFormatsBlittable(src_surface->pixel_format, dst_surface->pixel_format)) {
        return false;
    }

    ASSERT(src_rect.GetWidth() == dst_rect.GetWidth());

    const TextureCopy texture_copy = {
        .src_level = src_surface->LevelOf(src_params.addr),
        .dst_level = dst_surface->LevelOf(dst_params.addr),
        .src_offset = {src_rect.left, src_rect.bottom},
        .dst_offset = {dst_rect.left, dst_rect.bottom},
        .extent = {src_rect.GetWidth(), src_rect.GetHeight()},
    };
    runtime.CopyTextures(*src_surface, *dst_surface, texture_copy);

    InvalidateRegion(dst_params.addr, dst_params.size, dst_surface);
    return true;
}

bool RasterizerCache::AccelerateDisplayTransfer(const GPU::Regs::DisplayTransferConfig& config) {
    SurfaceParams src_params;
    src_params.addr = config.GetPhysicalInputAddress();
    src_params.width = config.output_width;
    src_params.stride = config.input_width;
    src_params.height = config.output_height;
    src_params.is_tiled = !config.input_linear;
    src_params.pixel_format = PixelFormatFromGPUPixelFormat(config.input_format);
    src_params.UpdateParams();

    SurfaceParams dst_params;
    dst_params.addr = config.GetPhysicalOutputAddress();
    dst_params.width = config.scaling != config.NoScale ? config.output_width.Value() / 2
                                                        : config.output_width.Value();
    dst_params.height = config.scaling == config.ScaleXY ? config.output_height.Value() / 2
                                                         : config.output_height.Value();
    dst_params.is_tiled = config.input_linear != config.dont_swizzle;
    dst_params.pixel_format = PixelFormatFromGPUPixelFormat(config.output_format);
    dst_params.UpdateParams();

    auto [src_surface, src_rect] = GetSurfaceSubRect(src_params, ScaleMatch::Ignore, true);
    if (!src_surface) {
        return false;
    }

    dst_params.res_scale = src_surface->res_scale;

    const auto [dst_surface, dst_rect] = GetSurfaceSubRect(dst_params, ScaleMatch::Upscale, false);
    if (!dst_surface) {
        return false;
    }

    if (src_surface->is_tiled != dst_surface->is_tiled) {
        std::swap(src_rect.top, src_rect.bottom);
    }
    if (config.flip_vertically) {
        std::swap(src_rect.top, src_rect.bottom);
    }

    if (!CheckFormatsBlittable(src_surface->pixel_format, dst_surface->pixel_format)) {
        return false;
    }

    const TextureBlit texture_blit = {
        .src_level = src_surface->LevelOf(src_params.addr),
        .dst_level = dst_surface->LevelOf(dst_params.addr),
        .src_rect = src_rect,
        .dst_rect = dst_rect,
    };
    runtime.BlitTextures(*src_surface, *dst_surface, texture_blit);

    InvalidateRegion(dst_params.addr, dst_params.size, dst_surface);
    return true;
}

bool RasterizerCache::AccelerateFill(const GPU::Regs::MemoryFillConfig& config) {
    SurfaceParams params;
    params.addr = config.GetStartAddress();
    params.end = config.GetEndAddress();
    params.size = params.end - params.addr;
    params.type = SurfaceType::Fill;
    params.res_scale = std::numeric_limits<u16>::max();

    SurfaceRef fill_surface = std::make_shared<OpenGL::Surface>(runtime, params);

    std::memcpy(&fill_surface->fill_data[0], &config.value_32bit, sizeof(u32));
    if (config.fill_32bit) {
        fill_surface->fill_size = 4;
    } else if (config.fill_24bit) {
        fill_surface->fill_size = 3;
    } else {
        fill_surface->fill_size = 2;
    }

    RegisterSurface(fill_surface);
    InvalidateRegion(fill_surface->addr, fill_surface->size, fill_surface);
    return true;
}

void RasterizerCache::CopySurface(const SurfaceRef& src_surface, const SurfaceRef& dst_surface,
                                  SurfaceInterval copy_interval) {
    MICROPROFILE_SCOPE(RasterizerCache_CopySurface);

    const PAddr copy_addr = copy_interval.lower();
    const SurfaceParams subrect_params = dst_surface->FromInterval(copy_interval);
    const auto dst_rect = dst_surface->GetScaledSubRect(subrect_params);
    ASSERT(subrect_params.GetInterval() == copy_interval && src_surface != dst_surface);

    if (src_surface->type == SurfaceType::Fill) {
        const TextureClear clear = {
            .texture_level = dst_surface->LevelOf(copy_addr),
            .texture_rect = dst_rect,
            .value = src_surface->MakeClearValue(copy_addr, dst_surface->pixel_format),
        };
        runtime.ClearTexture(*dst_surface, clear);
        return;
    }

    const TextureBlit blit = {
        .src_level = src_surface->LevelOf(copy_addr),
        .dst_level = dst_surface->LevelOf(copy_addr),
        .src_rect = src_surface->GetScaledSubRect(subrect_params),
        .dst_rect = dst_rect,
    };
    runtime.BlitTextures(*src_surface, *dst_surface, blit);
}

RasterizerCache::SurfaceRef RasterizerCache::GetSurface(const SurfaceParams& params,
                                                        ScaleMatch match_res_scale,
                                                        bool load_if_create) {
    if (params.addr == 0 || params.height * params.width == 0) {
        return nullptr;
    }
    // Use GetSurfaceSubRect instead
    ASSERT(params.width == params.stride);

    ASSERT(!params.is_tiled || (params.width % 8 == 0 && params.height % 8 == 0));

    // Check for an exact match in existing surfaces
    SurfaceRef surface = FindMatch<MatchFlags::Exact>(surface_cache, params, match_res_scale);

    if (!surface) {
        u16 target_res_scale = params.res_scale;
        if (match_res_scale != ScaleMatch::Exact) {
            // This surface may have a subrect of another surface with a higher res_scale, find
            // it to adjust our params
            SurfaceParams find_params = params;
            SurfaceRef expandable =
                FindMatch<MatchFlags::Expand>(surface_cache, find_params, match_res_scale);
            if (expandable && expandable->res_scale > target_res_scale) {
                target_res_scale = expandable->res_scale;
            }
            // Keep res_scale when reinterpreting d24s8 -> rgba8
            if (params.pixel_format == PixelFormat::RGBA8) {
                find_params.pixel_format = PixelFormat::D24S8;
                expandable =
                    FindMatch<MatchFlags::Expand>(surface_cache, find_params, match_res_scale);
                if (expandable && expandable->res_scale > target_res_scale) {
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

RasterizerCache::SurfaceRect_Tuple RasterizerCache::GetSurfaceSubRect(const SurfaceParams& params,
                                                                      ScaleMatch match_res_scale,
                                                                      bool load_if_create) {
    if (params.addr == 0 || params.height * params.width == 0) {
        return std::make_tuple(nullptr, Common::Rectangle<u32>{});
    }

    // Attempt to find encompassing surface
    SurfaceRef surface = FindMatch<MatchFlags::SubRect>(surface_cache, params, match_res_scale);

    // Check if FindMatch failed because of res scaling
    // If that's the case create a new surface with
    // the dimensions of the lower res_scale surface
    // to suggest it should not be used again
    if (!surface && match_res_scale != ScaleMatch::Ignore) {
        surface = FindMatch<MatchFlags::SubRect>(surface_cache, params, ScaleMatch::Ignore);
        if (surface) {
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
    if (!surface) {
        surface = FindMatch<MatchFlags::Expand>(surface_cache, aligned_params, match_res_scale);
        if (surface) {
            aligned_params.width = aligned_params.stride;
            aligned_params.UpdateParams();

            SurfaceParams new_params = *surface;
            new_params.addr = std::min(aligned_params.addr, surface->addr);
            new_params.end = std::max(aligned_params.end, surface->end);
            new_params.size = new_params.end - new_params.addr;
            new_params.height =
                new_params.size / aligned_params.BytesInPixels(aligned_params.stride);
            new_params.UpdateParams();
            ASSERT(new_params.size % aligned_params.BytesInPixels(aligned_params.stride) == 0);

            SurfaceRef new_surface = CreateSurface(new_params);
            DuplicateSurface(surface, new_surface);
            UnregisterSurface(surface);

            surface = new_surface;
            RegisterSurface(new_surface);
        }
    }

    // No subrect found - create and return a new surface
    if (!surface) {
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

RasterizerCache::SurfaceRef RasterizerCache::GetTextureSurface(
    const Pica::TexturingRegs::FullTextureConfig& config) {
    const auto info = Pica::Texture::TextureInfo::FromPicaRegister(config.config, config.format);
    const u32 max_level = MipLevels(info.width, info.height, config.config.lod.max_level) - 1;
    return GetTextureSurface(info, max_level);
}

RasterizerCache::SurfaceRef RasterizerCache::GetTextureSurface(
    const Pica::Texture::TextureInfo& info, u32 max_level) {
    if (info.physical_address == 0) [[unlikely]] {
        return nullptr;
    }

    SurfaceParams params;
    params.addr = info.physical_address;
    params.width = info.width;
    params.height = info.height;
    params.levels = max_level + 1;
    params.is_tiled = true;
    params.pixel_format = PixelFormatFromTextureFormat(info.format);
    params.res_scale = use_filter ? resolution_scale_factor : 1;
    params.UpdateParams();

    u32 min_width = info.width >> max_level;
    u32 min_height = info.height >> max_level;
    if (min_width % 8 != 0 || min_height % 8 != 0) {
        // This code is for 8x4 and 4x4 textures (commonly used by games for health bar)
        // The implementation might not be accurate and needs further testing.
        if (min_width % 4 == 0 && min_height % 4 == 0 && min_width * min_height <= 32) {
            const auto [src_surface, rect] = GetSurfaceSubRect(params, ScaleMatch::Ignore, true);
            params.res_scale = src_surface->res_scale;
            SurfaceRef tmp_surface = CreateSurface(params);

            const TextureBlit blit = {
                .src_level = src_surface->LevelOf(params.addr),
                .dst_level = 0,
                .src_rect = rect,
                .dst_rect = tmp_surface->GetScaledRect(),
            };
            runtime.BlitTextures(*src_surface, *tmp_surface, blit);
            return tmp_surface;
        }

        LOG_CRITICAL(HW_GPU, "Texture size ({}x{}) is not multiple of 4", min_width, min_height);
        return nullptr;
    }
    if (info.width != (min_width << max_level) || info.height != (min_height << max_level)) {
        LOG_CRITICAL(HW_GPU, "Texture size ({}x{}) does not support required mipmap level ({})",
                     params.width, params.height, max_level);
        return nullptr;
    }

    return GetSurface(params, ScaleMatch::Ignore, true);
}

RasterizerCache::SurfaceRef RasterizerCache::GetTextureCube(const TextureCubeConfig& config) {
    auto [it, new_surface] = texture_cube_cache.try_emplace(config);
    TextureCube& cube = it->second;

    if (new_surface) {
        SurfaceParams cube_params = {
            .addr = config.px,
            .width = config.width,
            .height = config.width,
            .stride = config.width,
            .levels = config.levels,
            .res_scale = use_filter ? resolution_scale_factor : 1,
            .texture_type = TextureType::CubeMap,
            .pixel_format = PixelFormatFromTextureFormat(config.format),
            .type = SurfaceType::Texture,
        };
        cube_params.UpdateParams();
        cube.surface = CreateSurface(cube_params);
    }

    const u32 scaled_size = cube.surface->GetScaledWidth();
    const std::array addresses = {config.px, config.nx, config.py, config.ny, config.pz, config.nz};

    for (u32 i = 0; i < addresses.size(); i++) {
        if (!addresses[i]) {
            continue;
        }

        Pica::Texture::TextureInfo info = {
            .physical_address = addresses[i],
            .width = config.width,
            .height = config.width,
            .format = config.format,
        };
        info.SetDefaultStride();

        SurfaceRef& face_surface = cube.faces[i];
        if (!face_surface || !face_surface->registered) {
            face_surface = GetTextureSurface(info, config.levels - 1);
            ASSERT(face_surface->levels == config.levels);
        }
        if (cube.ticks[i] != face_surface->ModificationTick()) {
            for (u32 level = 0; level < face_surface->levels; level++) {
                const TextureCopy texture_copy = {
                    .src_level = level,
                    .dst_level = level,
                    .src_layer = 0,
                    .dst_layer = i,
                    .src_offset = {0, 0},
                    .dst_offset = {0, 0},
                    .extent = {scaled_size >> level, scaled_size >> level},
                };
                runtime.CopyTextures(*face_surface, *cube.surface, texture_copy);
            }
            cube.ticks[i] = face_surface->ModificationTick();
        }
    }

    return cube.surface;
}

OpenGL::Framebuffer RasterizerCache::GetFramebufferSurfaces(bool using_color_fb,
                                                            bool using_depth_fb) {
    const auto& config = regs.framebuffer.framebuffer;

    const s32 framebuffer_width = config.GetWidth();
    const s32 framebuffer_height = config.GetHeight();
    const auto viewport_rect = regs.rasterizer.GetViewportRect();
    const Common::Rectangle<u32> viewport_clamped = {
        static_cast<u32>(std::clamp(viewport_rect.left, 0, framebuffer_width)),
        static_cast<u32>(std::clamp(viewport_rect.top, 0, framebuffer_height)),
        static_cast<u32>(std::clamp(viewport_rect.right, 0, framebuffer_width)),
        static_cast<u32>(std::clamp(viewport_rect.bottom, 0, framebuffer_height)),
    };

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
        LOG_CRITICAL(HW_GPU, "Color and depth framebuffer memory regions overlap; "
                             "overlapping framebuffers not supported!");
        using_depth_fb = false;
    }

    Common::Rectangle<u32> color_rect{};
    SurfaceRef color_surface = nullptr;
    u32 color_level{};
    if (using_color_fb)
        std::tie(color_surface, color_rect) =
            GetSurfaceSubRect(color_params, ScaleMatch::Exact, false);

    Common::Rectangle<u32> depth_rect{};
    SurfaceRef depth_surface = nullptr;
    u32 depth_level{};
    if (using_depth_fb)
        std::tie(depth_surface, depth_rect) =
            GetSurfaceSubRect(depth_params, ScaleMatch::Exact, false);

    Common::Rectangle<u32> fb_rect{};
    if (color_surface && depth_surface) {
        fb_rect = color_rect;
        // Color and Depth surfaces must have the same dimensions and offsets
        if (color_rect.bottom != depth_rect.bottom || color_rect.top != depth_rect.top ||
            color_rect.left != depth_rect.left || color_rect.right != depth_rect.right) {
            color_surface = GetSurface(color_params, ScaleMatch::Exact, false);
            depth_surface = GetSurface(depth_params, ScaleMatch::Exact, false);
            fb_rect = color_surface->GetScaledRect();
        }
    } else if (color_surface) {
        fb_rect = color_rect;
    } else if (depth_surface) {
        fb_rect = depth_rect;
    }

    if (color_surface) {
        color_level = color_surface->LevelOf(color_params.addr);
        ValidateSurface(color_surface, boost::icl::first(color_vp_interval),
                        boost::icl::length(color_vp_interval));
    }
    if (depth_surface) {
        depth_level = depth_surface->LevelOf(depth_params.addr);
        ValidateSurface(depth_surface, boost::icl::first(depth_vp_interval),
                        boost::icl::length(depth_vp_interval));
    }

    render_targets = RenderTargets{
        .color_surface = color_surface,
        .depth_surface = depth_surface,
    };

    return OpenGL::Framebuffer{
        runtime, color_surface.get(), color_level, depth_surface.get(), depth_level, regs, fb_rect};
}

void RasterizerCache::InvalidateFramebuffer(const OpenGL::Framebuffer& framebuffer) {
    if (framebuffer.HasAttachment(SurfaceType::Color)) {
        const auto interval = framebuffer.Interval(SurfaceType::Color);
        InvalidateRegion(boost::icl::first(interval), boost::icl::length(interval),
                         render_targets.color_surface);
    }
    if (framebuffer.HasAttachment(SurfaceType::DepthStencil)) {
        const auto interval = framebuffer.Interval(SurfaceType::DepthStencil);
        InvalidateRegion(boost::icl::first(interval), boost::icl::length(interval),
                         render_targets.depth_surface);
    }
}

RasterizerCache::SurfaceRect_Tuple RasterizerCache::GetTexCopySurface(const SurfaceParams& params) {
    Common::Rectangle<u32> rect{};

    SurfaceRef match_surface =
        FindMatch<MatchFlags::TexCopy>(surface_cache, params, ScaleMatch::Ignore);

    if (match_surface) {
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

void RasterizerCache::DuplicateSurface(const SurfaceRef& src_surface,
                                       const SurfaceRef& dest_surface) {
    ASSERT(dest_surface->addr <= src_surface->addr && dest_surface->end >= src_surface->end);

    const auto src_rect = src_surface->GetScaledRect();
    const auto dst_rect = dest_surface->GetScaledSubRect(*src_surface);
    ASSERT(src_rect.GetWidth() == dst_rect.GetWidth());

    const TextureCopy copy = {
        .src_level = 0,
        .dst_level = 0,
        .src_offset = {src_rect.left, src_rect.bottom},
        .dst_offset = {dst_rect.left, dst_rect.bottom},
        .extent = {src_rect.GetWidth(), src_rect.GetHeight()},
    };
    runtime.CopyTextures(*src_surface, *dest_surface, copy);

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

void RasterizerCache::ValidateSurface(const SurfaceRef& surface, PAddr addr, u32 size) {
    if (size == 0) [[unlikely]] {
        return;
    }

    const SurfaceInterval validate_interval(addr, addr + size);
    if (surface->type == SurfaceType::Fill) {
        ASSERT(surface->IsRegionValid(validate_interval));
        return;
    }

    SurfaceRegions validate_regions = surface->invalid_regions & validate_interval;

    auto notify_validated = [&](SurfaceInterval interval) {
        surface->MarkValid(interval);
        validate_regions.erase(interval);
    };

    u32 level = surface->LevelOf(addr);
    SurfaceInterval level_interval = surface->LevelInterval(level);
    while (!validate_regions.empty()) {
        // Take an invalid interval from the validation regions and clamp it
        // to the current level interval since FromInterval cannot process
        // intervals that span multiple levels. If the interval is empty
        // then we have validated the entire level so move to the next.
        const auto interval = *validate_regions.begin() & level_interval;
        if (boost::icl::is_empty(interval)) {
            level_interval = surface->LevelInterval(++level);
            continue;
        }

        // Look for a valid surface to copy from.
        const SurfaceParams params = surface->FromInterval(interval);
        const SurfaceRef copy_surface =
            FindMatch<MatchFlags::Copy>(surface_cache, params, ScaleMatch::Ignore, interval);

        if (copy_surface) {
            const SurfaceInterval copy_interval = copy_surface->GetCopyableInterval(params);
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
                LOG_DEBUG(HW_GPU, "Region created fully on GPU and reinterpretation is "
                                  "invalid. Skipping validation");
                validate_regions.erase(interval);
                continue;
            }
        }

        // Load data from 3DS memory
        FlushRegion(params.addr, params.size);
        UploadSurface(surface, interval);
        notify_validated(params.GetInterval());
    }

    // Filtered mipmaps often look really bad. We can achieve better quality by
    // generating them from the base level.
    if (surface->res_scale != 1 && level != 0) {
        runtime.GenerateMipmaps(*surface);
    }
}

void RasterizerCache::UploadSurface(const SurfaceRef& surface, SurfaceInterval interval) {
    MICROPROFILE_SCOPE(RasterizerCache_UploadSurface);

    const SurfaceParams load_info = surface->FromInterval(interval);
    ASSERT(load_info.addr >= surface->addr && load_info.end <= surface->end);

    const auto staging = runtime.FindStaging(
        load_info.width * load_info.height * surface->GetInternalBytesPerPixel(), true);

    MemoryRef source_ptr = memory.GetPhysicalRef(load_info.addr);
    if (!source_ptr) [[unlikely]] {
        return;
    }

    const auto upload_data = source_ptr.GetWriteBytes(load_info.end - load_info.addr);
    DecodeTexture(load_info, load_info.addr, load_info.end, upload_data, staging.mapped,
                  runtime.NeedsConversion(surface->pixel_format));

    if (use_custom_textures) {
        const u64 hash = ComputeCustomHash(load_info, staging.mapped, upload_data);
        if (UploadCustomSurface(surface, load_info, hash)) {
            return;
        }
    }
    if (dump_textures && !surface->is_custom) {
        const u64 hash = Common::ComputeHash64(upload_data.data(), upload_data.size());
        const u32 level = surface->LevelOf(load_info.addr);
        custom_tex_manager.DumpTexture(load_info, level, upload_data, hash);
    }

    const BufferTextureCopy upload = {
        .buffer_offset = 0,
        .buffer_size = staging.size,
        .texture_rect = surface->GetSubRect(load_info),
        .texture_level = surface->LevelOf(load_info.addr),
    };
    surface->Upload(upload, staging);
}

bool RasterizerCache::UploadCustomSurface(const SurfaceRef& surface, const SurfaceParams& load_info,
                                          u64 hash) {
    const u32 level = surface->LevelOf(load_info.addr);
    const bool is_base_level = level == 0;
    Material* material = custom_tex_manager.GetMaterial(hash);

    if (!material) {
        return surface->IsCustom();
    }
    if (!is_base_level && custom_tex_manager.SkipMipmaps()) {
        return true;
    }

    surface->is_custom = true;

    const auto upload = [this, level, surface, material]() -> bool {
        if (!surface->IsCustom() && !surface->Swap(material)) {
            LOG_ERROR(HW_GPU, "Custom compressed format {} unsupported by host GPU",
                      material->format);
            return false;
        }
        surface->UploadCustom(material, level);
        if (custom_tex_manager.SkipMipmaps()) {
            runtime.GenerateMipmaps(*surface);
        }
        return true;
    };
    return custom_tex_manager.Decode(material, std::move(upload));
}

u64 RasterizerCache::ComputeCustomHash(const SurfaceParams& load_info, std::span<u8> decoded,
                                       std::span<u8> upload_data) {
    MICROPROFILE_SCOPE(RasterizerCache_ComputeHash);

    if (custom_tex_manager.UseNewHash()) {
        return Common::ComputeHash64(upload_data.data(), upload_data.size());
    }
    return Common::ComputeHash64(decoded.data(), decoded.size());
}

void RasterizerCache::DownloadSurface(const SurfaceRef& surface, SurfaceInterval interval) {
    MICROPROFILE_SCOPE(RasterizerCache_DownloadSurface);

    const SurfaceParams flush_info = surface->FromInterval(interval);
    const u32 flush_start = boost::icl::first(interval);
    const u32 flush_end = boost::icl::last_next(interval);
    ASSERT(flush_start >= surface->addr && flush_end <= surface->end);

    const auto staging = runtime.FindStaging(
        flush_info.width * flush_info.height * surface->GetInternalBytesPerPixel(), false);

    const BufferTextureCopy download = {
        .buffer_offset = 0,
        .buffer_size = staging.size,
        .texture_rect = surface->GetSubRect(flush_info),
        .texture_level = surface->LevelOf(flush_start),
    };
    surface->Download(download, staging);

    MemoryRef dest_ptr = memory.GetPhysicalRef(flush_start);
    if (!dest_ptr) [[unlikely]] {
        return;
    }

    const auto download_dest = dest_ptr.GetWriteBytes(flush_end - flush_start);
    EncodeTexture(flush_info, flush_start, flush_end, staging.mapped, download_dest,
                  runtime.NeedsConversion(surface->pixel_format));
}

void RasterizerCache::DownloadFillSurface(const SurfaceRef& surface, SurfaceInterval interval) {
    const u32 flush_start = boost::icl::first(interval);
    const u32 flush_end = boost::icl::last_next(interval);
    ASSERT(flush_start >= surface->addr && flush_end <= surface->end);

    MemoryRef dest_ptr = memory.GetPhysicalRef(flush_start);
    if (!dest_ptr) [[unlikely]] {
        return;
    }

    const u32 start_offset = flush_start - surface->addr;
    const u32 download_size =
        std::clamp(flush_end - flush_start, 0u, static_cast<u32>(dest_ptr.GetSize()));
    const u32 coarse_start_offset = start_offset - (start_offset % surface->fill_size);
    const u32 backup_bytes = start_offset % surface->fill_size;

    std::array<u8, 4> backup_data;
    if (backup_bytes) {
        std::memcpy(backup_data.data(), &dest_ptr[coarse_start_offset], backup_bytes);
    }

    for (u32 offset = coarse_start_offset; offset < download_size; offset += surface->fill_size) {
        std::memcpy(&dest_ptr[offset], &surface->fill_data[0],
                    std::min(surface->fill_size, download_size - offset));
    }

    if (backup_bytes) {
        std::memcpy(&dest_ptr[coarse_start_offset], &backup_data[0], backup_bytes);
    }
}

bool RasterizerCache::NoUnimplementedReinterpretations(const SurfaceRef& surface,
                                                       SurfaceParams params,
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
            SurfaceRef test_surface =
                FindMatch<MatchFlags::Copy>(surface_cache, params, ScaleMatch::Ignore, interval);
            if (test_surface) {
                LOG_WARNING(HW_GPU, "Missing pixel_format reinterpreter: {} -> {}",
                            PixelFormatAsString(format),
                            PixelFormatAsString(surface->pixel_format));
                implemented = false;
            }
        }
    }
    return implemented;
}

bool RasterizerCache::IntervalHasInvalidPixelFormat(const SurfaceParams& params,
                                                    const SurfaceInterval& interval) {
    for (const auto& set : RangeFromInterval(surface_cache, interval)) {
        for (const auto& surface : set.second) {
            if (surface->pixel_format == PixelFormat::Invalid) {
                LOG_DEBUG(HW_GPU, "Surface {:#x} found with invalid pixel format", surface->addr);
                return true;
            }
        }
    }
    return false;
}

bool RasterizerCache::ValidateByReinterpretation(const SurfaceRef& surface, SurfaceParams params,
                                                 const SurfaceInterval& interval) {
    const PixelFormat dest_format = surface->pixel_format;
    for (const auto& reinterpreter : runtime.GetPossibleReinterpretations(dest_format)) {
        params.pixel_format = reinterpreter->GetSourceFormat();
        SurfaceRef reinterpret_surface =
            FindMatch<MatchFlags::Copy>(surface_cache, params, ScaleMatch::Ignore, interval);

        if (reinterpret_surface) {
            auto reinterpret_interval = reinterpret_surface->GetCopyableInterval(params);
            auto reinterpret_params = surface->FromInterval(reinterpret_interval);
            auto src_rect = reinterpret_surface->GetScaledSubRect(reinterpret_params);
            auto dest_rect = surface->GetScaledSubRect(reinterpret_params);
            reinterpreter->Reinterpret(*reinterpret_surface, src_rect, *surface, dest_rect);

            return true;
        }
    }

    return false;
}

void RasterizerCache::ClearAll(bool flush) {
    const auto flush_interval = PageMap::interval_type::right_open(0x0, 0xFFFFFFFF);
    // Force flush all surfaces from the cache
    if (flush) {
        FlushRegion(0x0, 0xFFFFFFFF);
    }
    // Unmark all of the marked pages
    for (auto& pair : RangeFromInterval(cached_pages, flush_interval)) {
        const auto interval = pair.first & flush_interval;

        const PAddr interval_start_addr = boost::icl::first(interval) << Memory::CITRA_PAGE_BITS;
        const PAddr interval_end_addr = boost::icl::last_next(interval) << Memory::CITRA_PAGE_BITS;
        const u32 interval_size = interval_end_addr - interval_start_addr;

        memory.RasterizerMarkRegionCached(interval_start_addr, interval_size, false);
    }

    // Remove the whole cache without really looking at it.
    cached_pages -= flush_interval;
    dirty_regions -= SurfaceInterval(0x0, 0xFFFFFFFF);
    surface_cache -= SurfaceInterval(0x0, 0xFFFFFFFF);
    remove_surfaces.clear();
}

void RasterizerCache::FlushRegion(PAddr addr, u32 size, SurfaceRef flush_surface) {
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

        if (flush_surface && surface != flush_surface)
            continue;

        // Sanity check, this surface is the last one that marked this region dirty
        ASSERT(surface->IsRegionValid(interval));

        if (surface->type == SurfaceType::Fill) {
            DownloadFillSurface(surface, interval);
        } else {
            DownloadSurface(surface, interval);
        }

        flushed_intervals += interval;
    }

    // Reset dirty regions
    dirty_regions -= flushed_intervals;
}

void RasterizerCache::FlushAll() {
    FlushRegion(0, 0xFFFFFFFF);
}

void RasterizerCache::InvalidateRegion(PAddr addr, u32 size, const SurfaceRef& region_owner) {
    if (size == 0)
        return;

    const SurfaceInterval invalid_interval(addr, addr + size);

    if (region_owner) {
        ASSERT(region_owner->type != SurfaceType::Texture);
        ASSERT(addr >= region_owner->addr && addr + size <= region_owner->end);
        // Surfaces can't have a gap
        ASSERT(region_owner->width == region_owner->stride);
        region_owner->MarkValid(invalid_interval);
    }

    for (const auto& pair : RangeFromInterval(surface_cache, invalid_interval)) {
        for (const auto& cached_surface : pair.second) {
            if (cached_surface == region_owner)
                continue;

            // If cpu is invalidating this region we want to remove it
            // to (likely) mark the memory pages as uncached
            if (!region_owner && size <= 8) {
                FlushRegion(cached_surface->addr, cached_surface->size, cached_surface);
                remove_surfaces.push_back(cached_surface);
                continue;
            }

            const auto interval = cached_surface->GetInterval() & invalid_interval;
            cached_surface->MarkInvalid(interval);

            // If the surface has no salvageable data it should be removed from the cache to avoid
            // clogging the data structure
            if (cached_surface->IsFullyInvalid()) {
                remove_surfaces.push_back(cached_surface);
            }
        }
    }

    if (region_owner) {
        dirty_regions.set({invalid_interval, region_owner});
    } else {
        dirty_regions.erase(invalid_interval);
    }

    for (const SurfaceRef& remove_surface : remove_surfaces) {
        UnregisterSurface(remove_surface);
    }
    remove_surfaces.clear();
}

RasterizerCache::SurfaceRef RasterizerCache::CreateSurface(const SurfaceParams& params) {
    SurfaceRef surface = std::make_shared<OpenGL::Surface>(runtime, params);
    surface->MarkInvalid(surface->GetInterval());
    return surface;
}

void RasterizerCache::RegisterSurface(const SurfaceRef& surface) {
    if (surface->registered) {
        return;
    }
    surface->registered = true;
    surface_cache.add({surface->GetInterval(), SurfaceSet{surface}});
    UpdatePagesCachedCount(surface->addr, surface->size, 1);
}

void RasterizerCache::UnregisterSurface(const SurfaceRef& surface) {
    if (!surface->registered) {
        return;
    }
    surface->registered = false;
    UpdatePagesCachedCount(surface->addr, surface->size, -1);
    surface_cache.subtract({surface->GetInterval(), SurfaceSet{surface}});
}

void RasterizerCache::UpdatePagesCachedCount(PAddr addr, u32 size, int delta) {
    const u32 num_pages =
        ((addr + size - 1) >> Memory::CITRA_PAGE_BITS) - (addr >> Memory::CITRA_PAGE_BITS) + 1;
    const u32 page_start = addr >> Memory::CITRA_PAGE_BITS;
    const u32 page_end = page_start + num_pages;

    // Interval maps will erase segments if count reaches 0, so if delta is negative we have to
    // subtract after iterating
    const auto pages_interval = PageMap::interval_type::right_open(page_start, page_end);
    if (delta > 0) {
        cached_pages.add({pages_interval, delta});
    }

    for (const auto& pair : RangeFromInterval(cached_pages, pages_interval)) {
        const auto interval = pair.first & pages_interval;
        const int count = pair.second;

        const PAddr interval_start_addr = boost::icl::first(interval) << Memory::CITRA_PAGE_BITS;
        const PAddr interval_end_addr = boost::icl::last_next(interval) << Memory::CITRA_PAGE_BITS;
        const u32 interval_size = interval_end_addr - interval_start_addr;

        if (delta > 0 && count == delta) {
            memory.RasterizerMarkRegionCached(interval_start_addr, interval_size, true);
        } else if (delta < 0 && count == -delta) {
            memory.RasterizerMarkRegionCached(interval_start_addr, interval_size, false);
        } else {
            ASSERT(count >= 0);
        }
    }

    if (delta < 0) {
        cached_pages.add({pages_interval, delta});
    }
}

} // namespace VideoCore
