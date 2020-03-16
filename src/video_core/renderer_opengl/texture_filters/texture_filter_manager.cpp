// Copyright 2019 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/logging/log.h"
#include "video_core/renderer_opengl/gl_state.h"
#include "video_core/renderer_opengl/texture_filters/anime4k/anime4k_ultrafast.h"
#include "video_core/renderer_opengl/texture_filters/bicubic/bicubic.h"
#include "video_core/renderer_opengl/texture_filters/texture_filter_manager.h"
#include "video_core/renderer_opengl/texture_filters/xbrz/xbrz_freescale.h"

namespace OpenGL {

Viewport TextureFilterInterface::RectToViewport(const Common::Rectangle<u32>& rect) {
    return {
        static_cast<GLint>(rect.left) * scale_factor,
        static_cast<GLint>(rect.top) * scale_factor,
        static_cast<GLsizei>(rect.GetWidth()) * scale_factor,
        static_cast<GLsizei>(rect.GetHeight()) * scale_factor,
    };
}

namespace {
template <typename T>
std::pair<std::string_view, TextureFilterInfo> FilterMapPair() {
    return {T::GetInfo().name, T::GetInfo()};
};

struct NoFilter {
    static TextureFilterInfo GetInfo() {
        TextureFilterInfo info;
        info.name = TextureFilterManager::NONE;
        info.clamp_scale = {1, 1};
        info.constructor = [](u16) { return nullptr; };
        return info;
    }
};
} // namespace

const std::map<std::string_view, TextureFilterInfo, TextureFilterManager::FilterNameComp>&
TextureFilterManager::TextureFilterMap() {
    static const std::map<std::string_view, TextureFilterInfo, FilterNameComp> filter_map{
        FilterMapPair<NoFilter>(),
        FilterMapPair<Anime4kUltrafast>(),
        FilterMapPair<Bicubic>(),
        FilterMapPair<XbrzFreescale>(),
    };
    return filter_map;
}

void TextureFilterManager::SetTextureFilter(std::string filter_name, u16 new_scale_factor) {
    if (name == filter_name && scale_factor == new_scale_factor)
        return;
    std::lock_guard<std::mutex> lock{mutex};
    name = std::move(filter_name);
    scale_factor = new_scale_factor;
    updated = true;
}

TextureFilterInterface* TextureFilterManager::GetTextureFilter() const {
    return filter.get();
}

bool TextureFilterManager::IsUpdated() const {
    return updated;
}

void TextureFilterManager::Reset() {
    std::lock_guard<std::mutex> lock{mutex};
    updated = false;
    auto iter = TextureFilterMap().find(name);
    if (iter == TextureFilterMap().end()) {
        LOG_ERROR(Render_OpenGL, "Invalid texture filter: {}", name);
        filter = nullptr;
        return;
    }

    const auto& filter_info = iter->second;

    u16 clamped_scale =
        std::clamp(scale_factor, filter_info.clamp_scale.min, filter_info.clamp_scale.max);
    if (clamped_scale != scale_factor)
        LOG_ERROR(Render_OpenGL, "Invalid scale factor {} for texture filter {}, clamped to {}",
                  scale_factor, filter_info.name, clamped_scale);

    filter = filter_info.constructor(clamped_scale);
}

} // namespace OpenGL
