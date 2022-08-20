// Copyright 2022 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include <functional>
#include "common/hash.h"
#include "video_core/rasterizer_cache/pixel_format.h"

namespace OpenGL {

struct FormatTuple {
    int internal_format;
    u32 format;
    u32 type;
};

const FormatTuple& GetFormatTuple(PixelFormat pixel_format);

struct HostTextureTag {
    FormatTuple format_tuple{};
    u32 width = 0;
    u32 height = 0;

    bool operator==(const HostTextureTag& rhs) const noexcept {
        return std::memcmp(this, &rhs, sizeof(HostTextureTag)) == 0;
    };

    const u64 Hash() const {
        return Common::ComputeHash64(this, sizeof(HostTextureTag));
    }
};

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
        return std::memcmp(this, &rhs, sizeof(TextureCubeConfig)) == 0;
    }

    bool operator!=(const TextureCubeConfig& rhs) const {
        return std::memcmp(this, &rhs, sizeof(TextureCubeConfig)) != 0;
    }

    const u64 Hash() const {
        return Common::ComputeHash64(this, sizeof(TextureCubeConfig));
    }
};

} // namespace OpenGL

namespace std {
template <>
struct hash<OpenGL::HostTextureTag> {
    std::size_t operator()(const OpenGL::HostTextureTag& tag) const noexcept {
        return tag.Hash();
    }
};

template <>
struct hash<OpenGL::TextureCubeConfig> {
    std::size_t operator()(const OpenGL::TextureCubeConfig& config) const noexcept {
        return config.Hash();
    }
};
} // namespace std
