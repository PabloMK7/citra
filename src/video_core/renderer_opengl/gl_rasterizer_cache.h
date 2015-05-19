// Copyright 2015 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "gl_state.h"
#include "gl_resource_manager.h"
#include "video_core/pica.h"

#include <memory>
#include <map>

class RasterizerCacheOpenGL : NonCopyable {
public:
    ~RasterizerCacheOpenGL();

    /// Loads a texture from 3DS memory to OpenGL and caches it (if not already cached)
    void LoadAndBindTexture(OpenGLState &state, unsigned texture_unit, const Pica::Regs::FullTextureConfig& config);

    /// Flush any cached resource that touches the flushed region
    void NotifyFlush(PAddr addr, u32 size);

    /// Flush all cached OpenGL resources tracked by this cache manager
    void FullFlush();

private:
    struct CachedTexture {
        OGLTexture texture;
        GLuint width;
        GLuint height;
        u32 size;
    };

    std::map<PAddr, std::unique_ptr<CachedTexture>> texture_cache;
};
