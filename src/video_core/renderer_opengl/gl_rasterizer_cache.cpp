// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/hash.h"
#include "common/make_unique.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "common/vector_math.h"

#include "core/memory.h"

#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/pica_to_gl.h"
#include "video_core/debug_utils/debug_utils.h"

RasterizerCacheOpenGL::~RasterizerCacheOpenGL() {
    FullFlush();
}

MICROPROFILE_DEFINE(OpenGL_TextureUpload, "OpenGL", "Texture Upload", MP_RGB(128, 64, 192));

void RasterizerCacheOpenGL::LoadAndBindTexture(OpenGLState &state, unsigned texture_unit, const Pica::Regs::FullTextureConfig& config) {
    PAddr texture_addr = config.config.GetPhysicalAddress();
    const auto cached_texture = texture_cache.find(texture_addr);

    if (cached_texture != texture_cache.end()) {
        state.texture_units[texture_unit].texture_2d = cached_texture->second->texture.handle;
        state.Apply();
    } else {
        MICROPROFILE_SCOPE(OpenGL_TextureUpload);

        std::unique_ptr<CachedTexture> new_texture = Common::make_unique<CachedTexture>();

        new_texture->texture.Create();
        state.texture_units[texture_unit].texture_2d = new_texture->texture.handle;
        state.Apply();
        glActiveTexture(GL_TEXTURE0 + texture_unit);

        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, PicaToGL::TextureFilterMode(config.config.mag_filter));
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, PicaToGL::TextureFilterMode(config.config.min_filter));

        GLenum wrap_s = PicaToGL::WrapMode(config.config.wrap_s);
        GLenum wrap_t = PicaToGL::WrapMode(config.config.wrap_t);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, wrap_s);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, wrap_t);

        if (wrap_s == GL_CLAMP_TO_BORDER || wrap_t == GL_CLAMP_TO_BORDER) {
            auto border_color = PicaToGL::ColorRGBA8((u8*)&config.config.border_color.r);
            glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, border_color.data());
        }

        const auto info = Pica::DebugUtils::TextureInfo::FromPicaRegister(config.config, config.format);
        u8* texture_src_data = Memory::GetPhysicalPointer(texture_addr);

        new_texture->width = info.width;
        new_texture->height = info.height;
        new_texture->size = info.stride * info.height;
        new_texture->addr = texture_addr;
        new_texture->hash = Common::ComputeHash64(texture_src_data, new_texture->size);

        std::unique_ptr<Math::Vec4<u8>[]> temp_texture_buffer_rgba(new Math::Vec4<u8>[info.width * info.height]);

        for (int y = 0; y < info.height; ++y) {
            for (int x = 0; x < info.width; ++x) {
                temp_texture_buffer_rgba[x + info.width * y] = Pica::DebugUtils::LookupTexture(texture_src_data, x, info.height - 1 - y, info);
            }
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info.width, info.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, temp_texture_buffer_rgba.get());

        texture_cache.emplace(texture_addr, std::move(new_texture));
    }
}

void RasterizerCacheOpenGL::NotifyFlush(PAddr addr, u32 size, bool ignore_hash) {
    // Flush any texture that falls in the flushed region
    // TODO: Optimize by also inserting upper bound (addr + size) of each texture into the same map and also narrow using lower_bound
    auto cache_upper_bound = texture_cache.upper_bound(addr + size);

    for (auto it = texture_cache.begin(); it != cache_upper_bound;) {
        const auto& info = *it->second;

        // Flush the texture only if the memory region intersects and a change is detected
        if (MathUtil::IntervalsIntersect(addr, size, info.addr, info.size) &&
            (ignore_hash || info.hash != Common::ComputeHash64(Memory::GetPhysicalPointer(info.addr), info.size))) {

            it = texture_cache.erase(it);
        } else {
            ++it;
        }
    }
}

void RasterizerCacheOpenGL::FullFlush() {
    texture_cache.clear();
}
