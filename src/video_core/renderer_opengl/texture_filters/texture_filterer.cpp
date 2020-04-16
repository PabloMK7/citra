/// Copyright 2020 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <functional>
#include <unordered_map>
#include "common/logging/log.h"
#include "video_core/renderer_opengl/texture_filters/anime4k/anime4k_ultrafast.h"
#include "video_core/renderer_opengl/texture_filters/bicubic/bicubic.h"
#include "video_core/renderer_opengl/texture_filters/scale_force/scale_force.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_base.h"
#include "video_core/renderer_opengl/texture_filters/texture_filterer.h"
#include "video_core/renderer_opengl/texture_filters/xbrz/xbrz_freescale.h"

namespace OpenGL {

namespace {

using TextureFilterContructor = std::function<std::unique_ptr<TextureFilterBase>(u16)>;

template <typename T>
std::pair<std::string_view, TextureFilterContructor> FilterMapPair() {
    return {T::NAME, std::make_unique<T, u16>};
};

static const std::unordered_map<std::string_view, TextureFilterContructor> filter_map{
    {TextureFilterer::NONE, [](u16) { return nullptr; }},
    FilterMapPair<Anime4kUltrafast>(),
    FilterMapPair<Bicubic>(),
    FilterMapPair<ScaleForce>(),
    FilterMapPair<XbrzFreescale>(),
};

} // namespace

TextureFilterer::TextureFilterer(std::string_view filter_name, u16 scale_factor) {
    Reset(filter_name, scale_factor);
}

bool TextureFilterer::Reset(std::string_view new_filter_name, u16 new_scale_factor) {
    if (filter_name == new_filter_name && (IsNull() || filter->scale_factor == new_scale_factor))
        return false;

    auto iter = filter_map.find(new_filter_name);
    if (iter == filter_map.end()) {
        LOG_ERROR(Render_OpenGL, "Invalid texture filter: {}", new_filter_name);
        filter = nullptr;
        return true;
    }

    filter_name = iter->first;
    filter = iter->second(new_scale_factor);
    return true;
}

bool TextureFilterer::IsNull() const {
    return !filter;
}

bool TextureFilterer::Filter(GLuint src_tex, const Common::Rectangle<u32>& src_rect, GLuint dst_tex,
                             const Common::Rectangle<u32>& dst_rect,
                             SurfaceParams::SurfaceType type, GLuint read_fb_handle,
                             GLuint draw_fb_handle) {
    // depth / stencil texture filtering is not supported for now
    if (IsNull() ||
        (type != SurfaceParams::SurfaceType::Color && type != SurfaceParams::SurfaceType::Texture))
        return false;
    filter->Filter(src_tex, src_rect, dst_tex, dst_rect, read_fb_handle, draw_fb_handle);
    return true;
}

std::vector<std::string_view> TextureFilterer::GetFilterNames() {
    std::vector<std::string_view> ret;
    std::transform(filter_map.begin(), filter_map.end(), std::back_inserter(ret),
                   [](auto pair) { return pair.first; });
    std::sort(ret.begin(), ret.end(), [](std::string_view lhs, std::string_view rhs) {
        // sort lexicographically with none at the top
        bool lhs_is_none{lhs == NONE};
        bool rhs_is_none{rhs == NONE};
        if (lhs_is_none || rhs_is_none)
            return lhs_is_none && !rhs_is_none;
        return lhs < rhs;
    });
    return ret;
}

} // namespace OpenGL