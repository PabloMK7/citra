// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <memory>

#include "common/hash.h"
#include "common/math_util.h"
#include "common/microprofile.h"
#include "common/vector_math.h"

#include "core/memory.h"

#include "video_core/debug_utils/debug_utils.h"
#include "video_core/renderer_opengl/gl_rasterizer_cache.h"
#include "video_core/renderer_opengl/pica_to_gl.h"

RasterizerCacheOpenGL::~RasterizerCacheOpenGL() {
    InvalidateAll();
}

MICROPROFILE_DEFINE(OpenGL_TextureUpload, "OpenGL", "Texture Upload", MP_RGB(128, 64, 192));

void RasterizerCacheOpenGL::LoadAndBindTexture(OpenGLState &state, unsigned texture_unit, const Pica::DebugUtils::TextureInfo& info) {
    const auto cached_texture = texture_cache.find(info.physical_address);

    if (cached_texture != texture_cache.end()) {
        state.texture_units[texture_unit].texture_2d = cached_texture->second->texture.handle;
        state.Apply();
    } else {
        MICROPROFILE_SCOPE(OpenGL_TextureUpload);

        std::unique_ptr<CachedTexture> new_texture = std::make_unique<CachedTexture>();

        new_texture->texture.Create();
        state.texture_units[texture_unit].texture_2d = new_texture->texture.handle;
        state.Apply();
        glActiveTexture(GL_TEXTURE0 + texture_unit);

        u8* texture_src_data = Memory::GetPhysicalPointer(info.physical_address);

        new_texture->width = info.width;
        new_texture->height = info.height;
        new_texture->size = info.stride * info.height;
        new_texture->addr = info.physical_address;
        new_texture->hash = Common::ComputeHash64(texture_src_data, new_texture->size);

        std::unique_ptr<Math::Vec4<u8>[]> temp_texture_buffer_rgba(new Math::Vec4<u8>[info.width * info.height]);

        for (int y = 0; y < info.height; ++y) {
            for (int x = 0; x < info.width; ++x) {
                temp_texture_buffer_rgba[x + info.width * y] = Pica::DebugUtils::LookupTexture(texture_src_data, x, info.height - 1 - y, info);
            }
        }

        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, info.width, info.height, 0, GL_RGBA, GL_UNSIGNED_BYTE, temp_texture_buffer_rgba.get());

        texture_cache.emplace(info.physical_address, std::move(new_texture));
    }
}

void RasterizerCacheOpenGL::InvalidateInRange(PAddr addr, u32 size, bool ignore_hash) {
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

void RasterizerCacheOpenGL::InvalidateAll() {
    texture_cache.clear();
}
