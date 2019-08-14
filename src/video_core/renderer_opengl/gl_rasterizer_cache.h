// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <array>
#include <list>
#include <memory>
#include <set>
#include <tuple>
#ifdef __GNUC__
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-local-typedefs"
#endif
#include <boost/icl/interval_map.hpp>
#include <boost/icl/interval_set.hpp>
#ifdef __GNUC__
#pragma GCC diagnostic pop
#endif
#include <unordered_map>
#include <boost/functional/hash.hpp>
#include <glad/glad.h>
#include "common/assert.h"
#include "common/common_funcs.h"
#include "common/common_types.h"
#include "common/math_util.h"
#include "core/custom_tex_cache.h"
#include "core/hw/gpu.h"
#include "video_core/regs_framebuffer.h"
#include "video_core/regs_texturing.h"
#include "video_core/renderer_opengl/gl_resource_manager.h"
#include "video_core/texture/texture_decode.h"

namespace OpenGL {

struct TextureCubeConfig {
    PAddr px;
    PAddr nx;
    PAddr py;
    PAddr ny;
    PAddr pz;
    PAddr nz;
    u32 width;
    Pica::TexturingRegs::TextureFormat format;

    bool operator==(const TextureCubeConfig& rhs) const {
        return std::tie(px, nx, py, ny, pz, nz, width, format) ==
               std::tie(rhs.px, rhs.nx, rhs.py, rhs.ny, rhs.pz, rhs.nz, rhs.width, rhs.format);
    }

    bool operator!=(const TextureCubeConfig& rhs) const {
        return !(*this == rhs);
    }
};

} // namespace OpenGL

namespace std {
template <>
struct hash<OpenGL::TextureCubeConfig> {
    std::size_t operator()(const OpenGL::TextureCubeConfig& config) const {
        std::size_t hash = 0;
        boost::hash_combine(hash, config.px);
        boost::hash_combine(hash, config.nx);
        boost::hash_combine(hash, config.py);
        boost::hash_combine(hash, config.ny);
        boost::hash_combine(hash, config.pz);
        boost::hash_combine(hash, config.nz);
        boost::hash_combine(hash, config.width);
        boost::hash_combine(hash, static_cast<u32>(config.format));
        return hash;
    }
};
} // namespace std

namespace OpenGL {

struct CachedSurface;
using Surface = std::shared_ptr<CachedSurface>;
using SurfaceSet = std::set<Surface>;

using SurfaceRegions = boost::icl::interval_set<PAddr>;
using SurfaceMap = boost::icl::interval_map<PAddr, Surface>;
using SurfaceCache = boost::icl::interval_map<PAddr, SurfaceSet>;

using SurfaceInterval = SurfaceCache::interval_type;
static_assert(std::is_same<SurfaceRegions::interval_type, SurfaceCache::interval_type>() &&
                  std::is_same<SurfaceMap::interval_type, SurfaceCache::interval_type>(),
              "incorrect interval types");

using SurfaceRect_Tuple = std::tuple<Surface, Common::Rectangle<u32>>;
using SurfaceSurfaceRect_Tuple = std::tuple<Surface, Surface, Common::Rectangle<u32>>;

using PageMap = boost::icl::interval_map<u32, int>;

enum class ScaleMatch {
    Exact,   // only accept same res scale
    Upscale, // only allow higher scale than params
    Ignore   // accept every scaled res
};

struct SurfaceParams {
    enum class PixelFormat {
        // First 5 formats are shared between textures and color buffers
        RGBA8 = 0,
        RGB8 = 1,
        RGB5A1 = 2,
        RGB565 = 3,
        RGBA4 = 4,

        // Texture-only formats
        IA8 = 5,
        RG8 = 6,
        I8 = 7,
        A8 = 8,
        IA4 = 9,
        I4 = 10,
        A4 = 11,
        ETC1 = 12,
        ETC1A4 = 13,

        // Depth buffer-only formats
        D16 = 14,
        // gap
        D24 = 16,
        D24S8 = 17,

        Invalid = 255,
    };

    enum class SurfaceType {
        Color = 0,
        Texture = 1,
        Depth = 2,
        DepthStencil = 3,
        Fill = 4,
        Invalid = 5
    };

    static constexpr unsigned int GetFormatBpp(PixelFormat format) {
        constexpr std::array<unsigned int, 18> bpp_table = {
            32, // RGBA8
            24, // RGB8
            16, // RGB5A1
            16, // RGB565
            16, // RGBA4
            16, // IA8
            16, // RG8
            8,  // I8
            8,  // A8
            8,  // IA4
            4,  // I4
            4,  // A4
            4,  // ETC1
            8,  // ETC1A4
            16, // D16
            0,
            24, // D24
            32, // D24S8
        };

        assert(static_cast<std::size_t>(format) < bpp_table.size());
        return bpp_table[static_cast<std::size_t>(format)];
    }
    unsigned int GetFormatBpp() const {
        return GetFormatBpp(pixel_format);
    }

    static PixelFormat PixelFormatFromTextureFormat(Pica::TexturingRegs::TextureFormat format) {
        return ((unsigned int)format < 14) ? (PixelFormat)format : PixelFormat::Invalid;
    }

    static PixelFormat PixelFormatFromColorFormat(Pica::FramebufferRegs::ColorFormat format) {
        return ((unsigned int)format < 5) ? (PixelFormat)format : PixelFormat::Invalid;
    }

    static PixelFormat PixelFormatFromDepthFormat(Pica::FramebufferRegs::DepthFormat format) {
        return ((unsigned int)format < 4) ? (PixelFormat)((unsigned int)format + 14)
                                          : PixelFormat::Invalid;
    }

    static PixelFormat PixelFormatFromGPUPixelFormat(GPU::Regs::PixelFormat format) {
        switch (format) {
        // RGB565 and RGB5A1 are switched in PixelFormat compared to ColorFormat
        case GPU::Regs::PixelFormat::RGB565:
            return PixelFormat::RGB565;
        case GPU::Regs::PixelFormat::RGB5A1:
            return PixelFormat::RGB5A1;
        default:
            return ((unsigned int)format < 5) ? (PixelFormat)format : PixelFormat::Invalid;
        }
    }

    static bool CheckFormatsBlittable(PixelFormat pixel_format_a, PixelFormat pixel_format_b) {
        SurfaceType a_type = GetFormatType(pixel_format_a);
        SurfaceType b_type = GetFormatType(pixel_format_b);

        if ((a_type == SurfaceType::Color || a_type == SurfaceType::Texture) &&
            (b_type == SurfaceType::Color || b_type == SurfaceType::Texture)) {
            return true;
        }

        if (a_type == SurfaceType::Depth && b_type == SurfaceType::Depth) {
            return true;
        }

        if (a_type == SurfaceType::DepthStencil && b_type == SurfaceType::DepthStencil) {
            return true;
        }

        return false;
    }

    static constexpr SurfaceType GetFormatType(PixelFormat pixel_format) {
        if ((unsigned int)pixel_format < 5) {
            return SurfaceType::Color;
        }

        if ((unsigned int)pixel_format < 14) {
            return SurfaceType::Texture;
        }

        if (pixel_format == PixelFormat::D16 || pixel_format == PixelFormat::D24) {
            return SurfaceType::Depth;
        }

        if (pixel_format == PixelFormat::D24S8) {
            return SurfaceType::DepthStencil;
        }

        return SurfaceType::Invalid;
    }

    /// Update the params "size", "end" and "type" from the already set "addr", "width", "height"
    /// and "pixel_format"
    void UpdateParams() {
        if (stride == 0) {
            stride = width;
        }
        type = GetFormatType(pixel_format);
        size = !is_tiled ? BytesInPixels(stride * (height - 1) + width)
                         : BytesInPixels(stride * 8 * (height / 8 - 1) + width * 8);
        end = addr + size;
    }

    SurfaceInterval GetInterval() const {
        return SurfaceInterval::right_open(addr, end);
    }

    // Returns the outer rectangle containing "interval"
    SurfaceParams FromInterval(SurfaceInterval interval) const;

    SurfaceInterval GetSubRectInterval(Common::Rectangle<u32> unscaled_rect) const;

    // Returns the region of the biggest valid rectange within interval
    SurfaceInterval GetCopyableInterval(const Surface& src_surface) const;

    u32 GetScaledWidth() const {
        return width * res_scale;
    }

    u32 GetScaledHeight() const {
        return height * res_scale;
    }

    Common::Rectangle<u32> GetRect() const {
        return {0, height, width, 0};
    }

    Common::Rectangle<u32> GetScaledRect() const {
        return {0, GetScaledHeight(), GetScaledWidth(), 0};
    }

    u32 PixelsInBytes(u32 size) const {
        return size * CHAR_BIT / GetFormatBpp(pixel_format);
    }

    u32 BytesInPixels(u32 pixels) const {
        return pixels * GetFormatBpp(pixel_format) / CHAR_BIT;
    }

    bool ExactMatch(const SurfaceParams& other_surface) const;
    bool CanSubRect(const SurfaceParams& sub_surface) const;
    bool CanExpand(const SurfaceParams& expanded_surface) const;
    bool CanTexCopy(const SurfaceParams& texcopy_params) const;

    Common::Rectangle<u32> GetSubRect(const SurfaceParams& sub_surface) const;
    Common::Rectangle<u32> GetScaledSubRect(const SurfaceParams& sub_surface) const;

    PAddr addr = 0;
    PAddr end = 0;
    u32 size = 0;

    u32 width = 0;
    u32 height = 0;
    u32 stride = 0;
    u16 res_scale = 1;

    bool is_tiled = false;
    PixelFormat pixel_format = PixelFormat::Invalid;
    SurfaceType type = SurfaceType::Invalid;
};

/**
 * A watcher that notifies whether a cached surface has been changed. This is useful for caching
 * surface collection objects, including texture cube and mipmap.
 */
struct SurfaceWatcher {
public:
    explicit SurfaceWatcher(std::weak_ptr<CachedSurface>&& surface) : surface(std::move(surface)) {}

    /**
     * Checks whether the surface has been changed.
     * @return false if the surface content has been changed since last Validate() call or has been
     * destroyed; otherwise true
     */
    bool IsValid() const {
        return !surface.expired() && valid;
    }

    /// Marks that the content of the referencing surface has been updated to the watcher user.
    void Validate() {
        ASSERT(!surface.expired());
        valid = true;
    }

    /// Gets the referencing surface. Returns null if the surface has been destroyed
    Surface Get() const {
        return surface.lock();
    }

private:
    friend struct CachedSurface;
    std::weak_ptr<CachedSurface> surface;
    bool valid = false;
};

struct CachedSurface : SurfaceParams, std::enable_shared_from_this<CachedSurface> {
    bool CanFill(const SurfaceParams& dest_surface, SurfaceInterval fill_interval) const;
    bool CanCopy(const SurfaceParams& dest_surface, SurfaceInterval copy_interval) const;

    bool IsRegionValid(SurfaceInterval interval) const {
        return (invalid_regions.find(interval) == invalid_regions.end());
    }

    bool IsSurfaceFullyInvalid() const {
        auto interval = GetInterval();
        return *invalid_regions.equal_range(interval).first == interval;
    }

    bool registered = false;
    SurfaceRegions invalid_regions;

    u32 fill_size = 0; /// Number of bytes to read from fill_data
    std::array<u8, 4> fill_data;

    OGLTexture texture;

    /// max mipmap level that has been attached to the texture
    u32 max_level = 0;
    /// level_watchers[i] watches the (i+1)-th level mipmap source surface
    std::array<std::shared_ptr<SurfaceWatcher>, 7> level_watchers;

    bool is_custom = false;
    Core::CustomTexInfo custom_tex_info;

    static constexpr unsigned int GetGLBytesPerPixel(PixelFormat format) {
        // OpenGL needs 4 bpp alignment for D24 since using GL_UNSIGNED_INT as type
        return format == PixelFormat::Invalid
                   ? 0
                   : (format == PixelFormat::D24 || GetFormatType(format) == SurfaceType::Texture)
                         ? 4
                         : SurfaceParams::GetFormatBpp(format) / 8;
    }

    std::unique_ptr<u8[]> gl_buffer;
    std::size_t gl_buffer_size = 0;

    // Read/Write data in 3DS memory to/from gl_buffer
    void LoadGLBuffer(PAddr load_start, PAddr load_end);
    void FlushGLBuffer(PAddr flush_start, PAddr flush_end);

    // Custom texture loading and dumping
    bool LoadCustomTexture(u64 tex_hash, Core::CustomTexInfo& tex_info,
                           Common::Rectangle<u32>& custom_rect);
    void DumpTexture(GLuint target_tex, u64 tex_hash);

    // Upload/Download data in gl_buffer in/to this surface's texture
    void UploadGLTexture(const Common::Rectangle<u32>& rect, GLuint read_fb_handle,
                         GLuint draw_fb_handle);
    void DownloadGLTexture(const Common::Rectangle<u32>& rect, GLuint read_fb_handle,
                           GLuint draw_fb_handle);

    std::shared_ptr<SurfaceWatcher> CreateWatcher() {
        auto watcher = std::make_shared<SurfaceWatcher>(weak_from_this());
        watchers.push_front(watcher);
        return watcher;
    }

    void InvalidateAllWatcher() {
        for (const auto& watcher : watchers) {
            if (auto locked = watcher.lock()) {
                locked->valid = false;
            }
        }
    }

    void UnlinkAllWatcher() {
        for (const auto& watcher : watchers) {
            if (auto locked = watcher.lock()) {
                locked->valid = false;
                locked->surface.reset();
            }
        }
        watchers.clear();
    }

private:
    std::list<std::weak_ptr<SurfaceWatcher>> watchers;
};

struct CachedTextureCube {
    OGLTexture texture;
    u16 res_scale = 1;
    std::shared_ptr<SurfaceWatcher> px;
    std::shared_ptr<SurfaceWatcher> nx;
    std::shared_ptr<SurfaceWatcher> py;
    std::shared_ptr<SurfaceWatcher> ny;
    std::shared_ptr<SurfaceWatcher> pz;
    std::shared_ptr<SurfaceWatcher> nz;
};

class RasterizerCacheOpenGL : NonCopyable {
public:
    RasterizerCacheOpenGL();
    ~RasterizerCacheOpenGL();

    /// Blit one surface's texture to another
    bool BlitSurfaces(const Surface& src_surface, const Common::Rectangle<u32>& src_rect,
                      const Surface& dst_surface, const Common::Rectangle<u32>& dst_rect);

    void ConvertD24S8toABGR(GLuint src_tex, const Common::Rectangle<u32>& src_rect, GLuint dst_tex,
                            const Common::Rectangle<u32>& dst_rect);

    /// Copy one surface's region to another
    void CopySurface(const Surface& src_surface, const Surface& dst_surface,
                     SurfaceInterval copy_interval);

    /// Load a texture from 3DS memory to OpenGL and cache it (if not already cached)
    Surface GetSurface(const SurfaceParams& params, ScaleMatch match_res_scale,
                       bool load_if_create);

    /// Attempt to find a subrect (resolution scaled) of a surface, otherwise loads a texture from
    /// 3DS memory to OpenGL and caches it (if not already cached)
    SurfaceRect_Tuple GetSurfaceSubRect(const SurfaceParams& params, ScaleMatch match_res_scale,
                                        bool load_if_create);

    /// Get a surface based on the texture configuration
    Surface GetTextureSurface(const Pica::TexturingRegs::FullTextureConfig& config);
    Surface GetTextureSurface(const Pica::Texture::TextureInfo& info, u32 max_level = 0);

    /// Get a texture cube based on the texture configuration
    const CachedTextureCube& GetTextureCube(const TextureCubeConfig& config);

    /// Get the color and depth surfaces based on the framebuffer configuration
    SurfaceSurfaceRect_Tuple GetFramebufferSurfaces(bool using_color_fb, bool using_depth_fb,
                                                    const Common::Rectangle<s32>& viewport_rect);

    /// Get a surface that matches the fill config
    Surface GetFillSurface(const GPU::Regs::MemoryFillConfig& config);

    /// Get a surface that matches a "texture copy" display transfer config
    SurfaceRect_Tuple GetTexCopySurface(const SurfaceParams& params);

    /// Write any cached resources overlapping the region back to memory (if dirty)
    void FlushRegion(PAddr addr, u32 size, Surface flush_surface = nullptr);

    /// Mark region as being invalidated by region_owner (nullptr if 3DS memory)
    void InvalidateRegion(PAddr addr, u32 size, const Surface& region_owner);

    /// Flush all cached resources tracked by this cache manager
    void FlushAll();

private:
    void DuplicateSurface(const Surface& src_surface, const Surface& dest_surface);

    /// Update surface's texture for given region when necessary
    void ValidateSurface(const Surface& surface, PAddr addr, u32 size);

    /// Create a new surface
    Surface CreateSurface(const SurfaceParams& params);

    /// Register surface into the cache
    void RegisterSurface(const Surface& surface);

    /// Remove surface from the cache
    void UnregisterSurface(const Surface& surface);

    /// Increase/decrease the number of surface in pages touching the specified region
    void UpdatePagesCachedCount(PAddr addr, u32 size, int delta);

    SurfaceCache surface_cache;
    PageMap cached_pages;
    SurfaceMap dirty_regions;
    SurfaceSet remove_surfaces;

    OGLFramebuffer read_framebuffer;
    OGLFramebuffer draw_framebuffer;

    OGLVertexArray attributeless_vao;
    OGLBuffer d24s8_abgr_buffer;
    GLsizeiptr d24s8_abgr_buffer_size;
    OGLProgram d24s8_abgr_shader;
    GLint d24s8_abgr_tbo_size_u_id;
    GLint d24s8_abgr_viewport_u_id;

    std::unordered_map<TextureCubeConfig, CachedTextureCube> texture_cube_cache;
};
} // namespace OpenGL
