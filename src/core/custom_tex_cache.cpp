// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "core/custom_tex_cache.h"

namespace Core {
CustomTexCache::CustomTexCache() = default;

CustomTexCache::~CustomTexCache() = default;

bool CustomTexCache::IsTextureDumped(u64 hash) const {
    return dumped_textures.count(hash);
}

void CustomTexCache::SetTextureDumped(const u64 hash) {
    dumped_textures.insert(hash);
}

bool CustomTexCache::IsTextureCached(u64 hash) const {
    return custom_textures.count(hash);
}

const CustomTexInfo& CustomTexCache::LookupTexture(u64 hash) const {
    return custom_textures.at(hash);
}

void CustomTexCache::CacheTexture(u64 hash, const std::vector<u8>& tex, u32 width, u32 height) {
    custom_textures[hash] = {width, height, tex};
}
} // namespace Core
