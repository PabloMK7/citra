// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <unordered_map>
#include <unordered_set>
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
    explicit CustomTexCache();
    ~CustomTexCache();

    bool IsTextureDumped(u64 hash) const;
    void SetTextureDumped(u64 hash);

    bool IsTextureCached(u64 hash) const;
    const CustomTexInfo& LookupTexture(u64 hash) const;
    void CacheTexture(u64 hash, const std::vector<u8>& tex, u32 width, u32 height);

private:
    std::unordered_set<u64> dumped_textures;
    std::unordered_map<u64, CustomTexInfo> custom_textures;
};
} // namespace Core