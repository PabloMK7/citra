#pragma once

#include <unordered_map>
#include <vector>
#include "common/common_types.h"

namespace Core {
struct CustomTexInfo {
    u32 width;
    u32 height;
    std::vector<u8> tex;
};

// TODO: think of a better name for this class...
class CustomTexCache {
public:
    const bool IsTextureDumped(const u64 hash);
    void SetTextureDumped(const u64 hash);

    const bool IsTextureCached(const u64 hash);
    const CustomTexInfo& LookupTexture(const u64 hash);
    void CacheTexture(const u64 hash, const std::vector<u8>& tex, u32 width, u32 height);

private:
    std::unordered_map<u64, bool> dumped_textures;
    std::unordered_map<u64, CustomTexInfo> custom_textures;
};
} // namespace Core