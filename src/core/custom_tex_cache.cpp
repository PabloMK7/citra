#include <stdexcept>
#include <vector>
#include "common/common_types.h"
#include "custom_tex_cache.h"

namespace Core {
const bool CustomTexCache::IsTextureDumped(const u64 hash) {
    return dumped_textures.find(hash) != dumped_textures.end();
}

void CustomTexCache::SetTextureDumped(const u64 hash) {
    dumped_textures[hash] = true;
}

const bool CustomTexCache::IsTextureCached(const u64 hash) {
    return custom_textures.find(hash) != custom_textures.end();
}

const CustomTexInfo& CustomTexCache::LookupTexture(const u64 hash) {
    return custom_textures.at(hash);
}

void CustomTexCache::CacheTexture(const u64 hash, const std::vector<u8>& tex, u32 width,
                                  u32 height) {
    custom_textures[hash] = {width, height, tex};
}
} // namespace Core