// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/microprofile.h"
#include "common/texture.h"
#include "common/scope_exit.h"
#include "core/core.h"
#include "video_core/rasterizer_cache/cached_surface.h"
#include "video_core/rasterizer_cache/morton_swizzle.h"
#include "video_core/rasterizer_cache/rasterizer_cache.h"
#include "video_core/renderer_opengl/texture_filters/texture_filterer.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/texture_downloader_es.h"

namespace OpenGL {

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

CachedSurface::~CachedSurface() {
    if (texture.handle) {
        auto tag = is_custom ? HostTextureTag{GetFormatTuple(PixelFormat::RGBA8),
                                              custom_tex_info.width, custom_tex_info.height}
                             : HostTextureTag{GetFormatTuple(pixel_format), GetScaledWidth(),
                                              GetScaledHeight()};

        owner.host_texture_recycler.emplace(tag, std::move(texture));
    }
}

MICROPROFILE_DEFINE(OpenGL_SurfaceLoad, "OpenGL", "Surface Load", MP_RGB(128, 192, 64));
void CachedSurface::LoadGLBuffer(PAddr load_start, PAddr load_end) {
    ASSERT(type != SurfaceType::Fill);
    const bool need_swap =
        GLES && (pixel_format == PixelFormat::RGBA8 || pixel_format == PixelFormat::RGB8);

    const u8* const texture_src_data = VideoCore::g_memory->GetPhysicalPointer(addr);
    if (texture_src_data == nullptr)
        return;

    if (gl_buffer.empty()) {
        gl_buffer.resize(width * height * GetBytesPerPixel(pixel_format));
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
        if (need_swap) {
            // TODO(liushuyu): check if the byteswap here is 100% correct
            // cannot fully test this
            if (pixel_format == PixelFormat::RGBA8) {
                for (std::size_t i = start_offset; i < load_end - addr; i += 4) {
                    gl_buffer[i] = texture_src_data[i + 3];
                    gl_buffer[i + 1] = texture_src_data[i + 2];
                    gl_buffer[i + 2] = texture_src_data[i + 1];
                    gl_buffer[i + 3] = texture_src_data[i];
                }
            } else if (pixel_format == PixelFormat::RGB8) {
                for (std::size_t i = start_offset; i < load_end - addr; i += 3) {
                    gl_buffer[i] = texture_src_data[i + 2];
                    gl_buffer[i + 1] = texture_src_data[i + 1];
                    gl_buffer[i + 2] = texture_src_data[i];
                }
            }
        } else {
            std::memcpy(&gl_buffer[start_offset], texture_src_data + start_offset,
                        load_end - load_start);
        }
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
    u8* const dst_buffer = VideoCore::g_memory->GetPhysicalPointer(addr);
    if (dst_buffer == nullptr)
        return;

    ASSERT(gl_buffer.size() == width * height * GetBytesPerPixel(pixel_format));

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
        if (pixel_format == PixelFormat::RGBA8 && GLES) {
            for (std::size_t i = start_offset; i < flush_end - addr; i += 4) {
                dst_buffer[i] = gl_buffer[i + 3];
                dst_buffer[i + 1] = gl_buffer[i + 2];
                dst_buffer[i + 2] = gl_buffer[i + 1];
                dst_buffer[i + 3] = gl_buffer[i];
            }
        } else if (pixel_format == PixelFormat::RGB8 && GLES) {
            for (std::size_t i = start_offset; i < flush_end - addr; i += 3) {
                dst_buffer[i] = gl_buffer[i + 2];
                dst_buffer[i + 1] = gl_buffer[i + 1];
                dst_buffer[i + 2] = gl_buffer[i];
            }
        } else {
            std::memcpy(dst_buffer + start_offset, &gl_buffer[start_offset],
                        flush_end - flush_start);
        }
    } else {
        gl_to_morton_fns[static_cast<std::size_t>(pixel_format)](stride, height, &gl_buffer[0],
                                                                 addr, flush_start, flush_end);
    }
}

bool CachedSurface::LoadCustomTexture(u64 tex_hash) {
    auto& custom_tex_cache = Core::System::GetInstance().CustomTexCache();
    const auto& image_interface = Core::System::GetInstance().GetImageInterface();

    if (custom_tex_cache.IsTextureCached(tex_hash)) {
        custom_tex_info = custom_tex_cache.LookupTexture(tex_hash);
        return true;
    }

    if (!custom_tex_cache.CustomTextureExists(tex_hash)) {
        return false;
    }

    const auto& path_info = custom_tex_cache.LookupTexturePathInfo(tex_hash);
    if (!image_interface->DecodePNG(custom_tex_info.tex, custom_tex_info.width,
                                    custom_tex_info.height, path_info.path)) {
        LOG_ERROR(Render_OpenGL, "Failed to load custom texture {}", path_info.path);
        return false;
    }

    const std::bitset<32> width_bits(custom_tex_info.width);
    const std::bitset<32> height_bits(custom_tex_info.height);
    if (width_bits.count() != 1 || height_bits.count() != 1) {
        LOG_ERROR(Render_OpenGL, "Texture {} size is not a power of 2", path_info.path);
        return false;
    }

    LOG_DEBUG(Render_OpenGL, "Loaded custom texture from {}", path_info.path);
    Common::FlipRGBA8Texture(custom_tex_info.tex, custom_tex_info.width, custom_tex_info.height);
    custom_tex_cache.CacheTexture(tex_hash, custom_tex_info.tex, custom_tex_info.width,
                                  custom_tex_info.height);
    return true;
}

void CachedSurface::DumpTexture(GLuint target_tex, u64 tex_hash) {
    // Make sure the texture size is a power of 2
    // If not, the surface is actually a framebuffer
    std::bitset<32> width_bits(width);
    std::bitset<32> height_bits(height);
    if (width_bits.count() != 1 || height_bits.count() != 1) {
        LOG_WARNING(Render_OpenGL, "Not dumping {:016X} because size isn't a power of 2 ({}x{})",
                    tex_hash, width, height);
        return;
    }

    // Dump texture to RGBA8 and encode as PNG
    const auto& image_interface = Core::System::GetInstance().GetImageInterface();
    auto& custom_tex_cache = Core::System::GetInstance().CustomTexCache();
    std::string dump_path =
        fmt::format("{}textures/{:016X}/", FileUtil::GetUserPath(FileUtil::UserPath::DumpDir),
                    Core::System::GetInstance().Kernel().GetCurrentProcess()->codeset->program_id);
    if (!FileUtil::CreateFullPath(dump_path)) {
        LOG_ERROR(Render, "Unable to create {}", dump_path);
        return;
    }

    dump_path += fmt::format("tex1_{}x{}_{:016X}_{}.png", width, height, tex_hash, pixel_format);
    if (!custom_tex_cache.IsTextureDumped(tex_hash) && !FileUtil::Exists(dump_path)) {
        custom_tex_cache.SetTextureDumped(tex_hash);

        LOG_INFO(Render_OpenGL, "Dumping texture to {}", dump_path);
        std::vector<u8> decoded_texture;
        decoded_texture.resize(width * height * 4);
        OpenGLState state = OpenGLState::GetCurState();
        GLuint old_texture = state.texture_units[0].texture_2d;
        state.Apply();
        /*
           GetTexImageOES is used even if not using OpenGL ES to work around a small issue that
           happens if using custom textures with texture dumping at the same.
           Let's say there's 2 textures that are both 32x32 and one of them gets replaced with a
           higher quality 256x256 texture. If the 256x256 texture is displayed first and the
           32x32 texture gets uploaded to the same underlying OpenGL texture, the 32x32 texture
           will appear in the corner of the 256x256 texture. If texture dumping is enabled and
           the 32x32 is undumped, Citra will attempt to dump it. Since the underlying OpenGL
           texture is still 256x256, Citra crashes because it thinks the texture is only 32x32.
           GetTexImageOES conveniently only dumps the specified region, and works on both
           desktop and ES.
        */
        // if the backend isn't OpenGL ES, this won't be initialized yet
        if (!owner.texture_downloader_es) {
            owner.texture_downloader_es = std::make_unique<TextureDownloaderES>(false);
        }

        owner.texture_downloader_es->GetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE,
                                                 height, width, &decoded_texture[0]);
        state.texture_units[0].texture_2d = old_texture;
        state.Apply();
        Common::FlipRGBA8Texture(decoded_texture, width, height);
        if (!image_interface->EncodePNG(dump_path, decoded_texture, width, height))
            LOG_ERROR(Render_OpenGL, "Failed to save decoded texture");
    }
}

MICROPROFILE_DEFINE(OpenGL_TextureUL, "OpenGL", "Texture Upload", MP_RGB(128, 192, 64));
void CachedSurface::UploadGLTexture(Common::Rectangle<u32> rect) {
    if (type == SurfaceType::Fill) {
        return;
    }

    MICROPROFILE_SCOPE(OpenGL_TextureUL);
    ASSERT(gl_buffer.size() == width * height * GetBytesPerPixel(pixel_format));

    u64 tex_hash = 0;

    if (Settings::values.dump_textures || Settings::values.custom_textures) {
        tex_hash = Common::ComputeHash64(gl_buffer.data(), gl_buffer.size());
    }

    if (Settings::values.custom_textures) {
        is_custom = LoadCustomTexture(tex_hash);
    }

    // Load data from memory to the surface
    GLint x0 = static_cast<GLint>(rect.left);
    GLint y0 = static_cast<GLint>(rect.bottom);
    std::size_t buffer_offset = (y0 * stride + x0) * GetBytesPerPixel(pixel_format);

    const FormatTuple& tuple = GetFormatTuple(pixel_format);
    GLuint target_tex = texture.handle;

    // If not 1x scale, create 1x texture that we will blit from to replace texture subrect in
    // surface
    OGLTexture unscaled_tex;
    if (res_scale != 1) {
        x0 = 0;
        y0 = 0;

        if (is_custom) {
            const auto& tuple = GetFormatTuple(PixelFormat::RGBA8);
            unscaled_tex = owner.AllocateSurfaceTexture(tuple, custom_tex_info.width,
                                                        custom_tex_info.height);
        } else {
            unscaled_tex = owner.AllocateSurfaceTexture(tuple, rect.GetWidth(), rect.GetHeight());
        }

        target_tex = unscaled_tex.handle;
    }

    OpenGLState cur_state = OpenGLState::GetCurState();

    GLuint old_tex = cur_state.texture_units[0].texture_2d;
    cur_state.texture_units[0].texture_2d = target_tex;
    cur_state.Apply();

    // Ensure no bad interactions with GL_UNPACK_ALIGNMENT
    ASSERT(stride * GetBytesPerPixel(pixel_format) % 4 == 0);
    if (is_custom) {
        if (res_scale == 1) {
            texture = owner.AllocateSurfaceTexture(GetFormatTuple(PixelFormat::RGBA8),
                                                   custom_tex_info.width, custom_tex_info.height);
            cur_state.texture_units[0].texture_2d = texture.handle;
            cur_state.Apply();
        }

        // Always going to be using rgba8
        glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(custom_tex_info.width));

        glActiveTexture(GL_TEXTURE0);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x0, y0, custom_tex_info.width, custom_tex_info.height,
                        GL_RGBA, GL_UNSIGNED_BYTE, custom_tex_info.tex.data());
    } else {
        glPixelStorei(GL_UNPACK_ROW_LENGTH, static_cast<GLint>(stride));

        glActiveTexture(GL_TEXTURE0);
        glTexSubImage2D(GL_TEXTURE_2D, 0, x0, y0, static_cast<GLsizei>(rect.GetWidth()),
                        static_cast<GLsizei>(rect.GetHeight()), tuple.format, tuple.type,
                        &gl_buffer[buffer_offset]);
    }

    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    if (Settings::values.dump_textures && !is_custom) {
        DumpTexture(target_tex, tex_hash);
    }

    cur_state.texture_units[0].texture_2d = old_tex;
    cur_state.Apply();

    if (res_scale != 1) {
        auto scaled_rect = rect;
        scaled_rect.left *= res_scale;
        scaled_rect.top *= res_scale;
        scaled_rect.right *= res_scale;
        scaled_rect.bottom *= res_scale;

        const u32 width = is_custom ? custom_tex_info.width : rect.GetWidth();
        const u32 height = is_custom ? custom_tex_info.height : rect.GetHeight();
        const Common::Rectangle<u32> from_rect{0, height, width, 0};

        if (!owner.texture_filterer->Filter(unscaled_tex, from_rect, texture, scaled_rect, type)) {
            const Aspect aspect = ToAspect(type);
            runtime.BlitTextures(unscaled_tex, {aspect, from_rect},
                                 texture, {aspect, scaled_rect});
        }
    }

    InvalidateAllWatcher();
}

MICROPROFILE_DEFINE(OpenGL_TextureDL, "OpenGL", "Texture Download", MP_RGB(128, 192, 64));
void CachedSurface::DownloadGLTexture(const Common::Rectangle<u32>& rect) {
    if (type == SurfaceType::Fill) {
        return;
    }

    MICROPROFILE_SCOPE(OpenGL_TextureDL);

    if (gl_buffer.empty()) {
        gl_buffer.resize(width * height * GetBytesPerPixel(pixel_format));
    }

    OpenGLState state = OpenGLState::GetCurState();
    OpenGLState prev_state = state;
    SCOPE_EXIT({ prev_state.Apply(); });

    const FormatTuple& tuple = GetFormatTuple(pixel_format);

    // Ensure no bad interactions with GL_PACK_ALIGNMENT
    ASSERT(stride * GetBytesPerPixel(pixel_format) % 4 == 0);
    glPixelStorei(GL_PACK_ROW_LENGTH, static_cast<GLint>(stride));
    const std::size_t buffer_offset = (rect.bottom * stride + rect.left) * GetBytesPerPixel(pixel_format);

    // If not 1x scale, blit scaled texture to a new 1x texture and use that to flush
    const Aspect aspect = ToAspect(type);
    if (res_scale != 1) {
        auto scaled_rect = rect;
        scaled_rect.left *= res_scale;
        scaled_rect.top *= res_scale;
        scaled_rect.right *= res_scale;
        scaled_rect.bottom *= res_scale;

        const Common::Rectangle<u32> unscaled_tex_rect{0, rect.GetHeight(), rect.GetWidth(), 0};
        auto unscaled_tex = owner.AllocateSurfaceTexture(tuple, rect.GetWidth(),
                                                         rect.GetHeight());
        // Blit scaled texture to the unscaled one
        runtime.BlitTextures(texture, {aspect, scaled_rect},
                             unscaled_tex, {aspect, unscaled_tex_rect});

        state.texture_units[0].texture_2d = unscaled_tex.handle;
        state.Apply();

        glActiveTexture(GL_TEXTURE0);
        if (GLES) {
            owner.texture_downloader_es->GetTexImage(GL_TEXTURE_2D, 0, tuple.format, tuple.type,
                                                     rect.GetHeight(), rect.GetWidth(),
                                                     &gl_buffer[buffer_offset]);
        } else {
            glGetTexImage(GL_TEXTURE_2D, 0, tuple.format, tuple.type, &gl_buffer[buffer_offset]);
        }
    } else {
        runtime.ReadTexture(texture, {aspect, rect}, tuple, gl_buffer.data());
    }

    glPixelStorei(GL_PACK_ROW_LENGTH, 0);
}

bool CachedSurface::CanFill(const SurfaceParams& dest_surface, SurfaceInterval fill_interval) const {
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

bool CachedSurface::CanCopy(const SurfaceParams& dest_surface, SurfaceInterval copy_interval) const {
    SurfaceParams subrect_params = dest_surface.FromInterval(copy_interval);
    ASSERT(subrect_params.GetInterval() == copy_interval);
    if (CanSubRect(subrect_params))
        return true;

    if (CanFill(dest_surface, copy_interval))
        return true;

    return false;
}

} // namespace OpenGL
