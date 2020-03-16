// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include <functional>
#include <string_view>
#include "common/common_types.h"
#include "common/math_util.h"

namespace OpenGL {

struct CachedSurface;
struct Viewport;

class TextureFilterInterface {
public:
    const u16 scale_factor{};
    TextureFilterInterface(u16 scale_factor) : scale_factor{scale_factor} {}
    virtual void scale(CachedSurface& surface, const Common::Rectangle<u32>& rect,
                       std::size_t buffer_offset) = 0;
    virtual ~TextureFilterInterface() = default;

protected:
    Viewport RectToViewport(const Common::Rectangle<u32>& rect);
};

// every texture filter should have a static GetInfo function
struct TextureFilterInfo {
    std::string_view name;
    struct {
        u16 min, max;
    } clamp_scale{1, 10};
    std::function<std::unique_ptr<TextureFilterInterface>(u16 scale_factor)> constructor;
};

} // namespace OpenGL
