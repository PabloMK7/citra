// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <unordered_map>
#include <vector>
#include <boost/icl/interval_map.hpp>
#include <boost/icl/interval_set.hpp>
#include "video_core/rasterizer_cache/surface_base.h"
#include "video_core/rasterizer_cache/surface_params.h"
#include "video_core/renderer_opengl/gl_texture_runtime.h"
#include "video_core/texture/texture_decode.h"

namespace Memory {
class MemorySystem;
}

namespace Pica {
struct Regs;
}

namespace VideoCore {

enum class ScaleMatch {
    Exact,   // only accept same res scale
    Upscale, // only allow higher scale than params
    Ignore   // accept every scaled res
};

class CustomTexManager;
class CustomTexture;
class RendererBase;

class RasterizerCache : NonCopyable {
public:
    using SurfaceRef = std::shared_ptr<OpenGL::Surface>;

    // Declare rasterizer interval types
    using SurfaceSet = std::set<SurfaceRef>;
    using SurfaceMap = boost::icl::interval_map<PAddr, SurfaceRef, boost::icl::partial_absorber,
                                                std::less, boost::icl::inplace_plus,
                                                boost::icl::inter_section, SurfaceInterval>;
    using SurfaceCache = boost::icl::interval_map<PAddr, SurfaceSet, boost::icl::partial_absorber,
                                                  std::less, boost::icl::inplace_plus,
                                                  boost::icl::inter_section, SurfaceInterval>;

    static_assert(std::is_same<SurfaceRegions::interval_type, SurfaceCache::interval_type>() &&
                      std::is_same<SurfaceMap::interval_type, SurfaceCache::interval_type>(),
                  "Incorrect interval types");

    using SurfaceRect_Tuple = std::tuple<SurfaceRef, Common::Rectangle<u32>>;
    using PageMap = boost::icl::interval_map<u32, int>;

    struct RenderTargets {
        SurfaceRef color_surface;
        SurfaceRef depth_surface;
    };

    struct TextureCube {
        SurfaceRef surface{};
        std::array<SurfaceRef, 6> faces{};
        std::array<u64, 6> ticks{};
    };

public:
    RasterizerCache(Memory::MemorySystem& memory, CustomTexManager& custom_tex_manager,
                    OpenGL::TextureRuntime& runtime, Pica::Regs& regs, RendererBase& renderer);
    ~RasterizerCache();

    /// Notify the cache that a new frame has been queued
    void TickFrame();

    /// Perform hardware accelerated texture copy according to the provided configuration
    bool AccelerateTextureCopy(const GPU::Regs::DisplayTransferConfig& config);

    /// Perform hardware accelerated display transfer according to the provided configuration
    bool AccelerateDisplayTransfer(const GPU::Regs::DisplayTransferConfig& config);

    /// Perform hardware accelerated memory fill according to the provided configuration
    bool AccelerateFill(const GPU::Regs::MemoryFillConfig& config);

    /// Copy one surface's region to another
    void CopySurface(const SurfaceRef& src_surface, const SurfaceRef& dst_surface,
                     SurfaceInterval copy_interval);

    /// Load a texture from 3DS memory to OpenGL and cache it (if not already cached)
    SurfaceRef GetSurface(const SurfaceParams& params, ScaleMatch match_res_scale,
                          bool load_if_create);

    /// Attempt to find a subrect (resolution scaled) of a surface, otherwise loads a texture from
    /// 3DS memory to OpenGL and caches it (if not already cached)
    SurfaceRect_Tuple GetSurfaceSubRect(const SurfaceParams& params, ScaleMatch match_res_scale,
                                        bool load_if_create);

    /// Get a surface based on the texture configuration
    SurfaceRef GetTextureSurface(const Pica::TexturingRegs::FullTextureConfig& config);
    SurfaceRef GetTextureSurface(const Pica::Texture::TextureInfo& info, u32 max_level = 0);

    /// Get a texture cube based on the texture configuration
    SurfaceRef GetTextureCube(const TextureCubeConfig& config);

    /// Get the color and depth surfaces based on the framebuffer configuration
    OpenGL::Framebuffer GetFramebufferSurfaces(bool using_color_fb, bool using_depth_fb);

    /// Marks the draw rectangle defined in framebuffer as invalid
    void InvalidateFramebuffer(const OpenGL::Framebuffer& framebuffer);

    /// Get a surface that matches a "texture copy" display transfer config
    SurfaceRect_Tuple GetTexCopySurface(const SurfaceParams& params);

    /// Write any cached resources overlapping the region back to memory (if dirty)
    void FlushRegion(PAddr addr, u32 size, SurfaceRef flush_surface = nullptr);

    /// Mark region as being invalidated by region_owner (nullptr if 3DS memory)
    void InvalidateRegion(PAddr addr, u32 size, const SurfaceRef& region_owner);

    /// Flush all cached resources tracked by this cache manager
    void FlushAll();

    /// Clear all cached resources tracked by this cache manager
    void ClearAll(bool flush);

private:
    /// Transfers ownership of a memory region from src_surface to dest_surface
    void DuplicateSurface(const SurfaceRef& src_surface, const SurfaceRef& dest_surface);

    /// Update surface's texture for given region when necessary
    void ValidateSurface(const SurfaceRef& surface, PAddr addr, u32 size);

    /// Copies pixel data in interval from the guest VRAM to the host GPU surface
    void UploadSurface(const SurfaceRef& surface, SurfaceInterval interval);

    /// Uploads a custom texture identified with hash to the target surface
    bool UploadCustomSurface(const SurfaceRef& surface, const SurfaceParams& load_info, u64 hash);

    /// Returns the hash used to lookup the custom surface.
    u64 ComputeCustomHash(const SurfaceParams& load_info, std::span<u8> decoded,
                          std::span<u8> upload_data);

    /// Copies pixel data in interval from the host GPU surface to the guest VRAM
    void DownloadSurface(const SurfaceRef& surface, SurfaceInterval interval);

    /// Downloads a fill surface to guest VRAM
    void DownloadFillSurface(const SurfaceRef& surface, SurfaceInterval interval);

    /// Returns false if there is a surface in the cache at the interval with the same bit-width,
    bool NoUnimplementedReinterpretations(const SurfaceRef& surface, SurfaceParams params,
                                          const SurfaceInterval& interval);

    /// Return true if a surface with an invalid pixel format exists at the interval
    bool IntervalHasInvalidPixelFormat(const SurfaceParams& params,
                                       const SurfaceInterval& interval);

    /// Attempt to find a reinterpretable surface in the cache and use it to copy for validation
    bool ValidateByReinterpretation(const SurfaceRef& surface, SurfaceParams params,
                                    const SurfaceInterval& interval);

    /// Create a new surface
    SurfaceRef CreateSurface(const SurfaceParams& params);

    /// Register surface into the cache
    void RegisterSurface(const SurfaceRef& surface);

    /// Remove surface from the cache
    void UnregisterSurface(const SurfaceRef& surface);

    /// Increase/decrease the number of surface in pages touching the specified region
    void UpdatePagesCachedCount(PAddr addr, u32 size, int delta);

private:
    Memory::MemorySystem& memory;
    CustomTexManager& custom_tex_manager;
    OpenGL::TextureRuntime& runtime;
    Pica::Regs& regs;
    RendererBase& renderer;
    SurfaceCache surface_cache;
    PageMap cached_pages;
    SurfaceMap dirty_regions;
    std::vector<SurfaceRef> remove_surfaces;
    u32 resolution_scale_factor;
    RenderTargets render_targets;
    std::unordered_map<TextureCubeConfig, TextureCube> texture_cube_cache;
    bool use_filter;
    bool dump_textures;
    bool use_custom_textures;
};

} // namespace VideoCore
