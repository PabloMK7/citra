// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <string>
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

// This is to avoid parsing the filename multiple times
struct CustomTexPathInfo {
    std::string path;
    u64 hash;
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

    void AddTexturePath(u64 hash, const std::string& path);
    void FindCustomTextures();
    void PreloadTextures();
    bool CustomTextureExists(u64 hash) const;
    const CustomTexPathInfo& LookupTexturePathInfo(u64 hash) const;
    bool IsTexturePathMapEmpty() const;

private:
    std::unordered_set<u64> dumped_textures;
    std::unordered_map<u64, CustomTexInfo> custom_textures;
    std::unordered_map<u64, CustomTexPathInfo> custom_texture_paths;
};
} // namespace Core
