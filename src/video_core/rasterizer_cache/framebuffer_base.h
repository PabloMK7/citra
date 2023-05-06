// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/math_util.h"
#include "video_core/rasterizer_cache/surface_params.h"

namespace Pica {
struct Regs;
}

namespace VideoCore {

class SurfaceBase;

struct ViewportInfo {
    s32 x;
    s32 y;
    s32 width;
    s32 height;
};

/**
 * A framebuffer is a lightweight abstraction over a pair of surfaces and provides
 * metadata about them.
 */
class FramebufferBase {
public:
    FramebufferBase();
    FramebufferBase(const Pica::Regs& regs, const SurfaceBase* color, u32 color_level,
                    const SurfaceBase* depth_stencil, u32 depth_level,
                    Common::Rectangle<u32> surfaces_rect);

    SurfaceParams ColorParams() const noexcept {
        return color_params;
    }

    SurfaceParams DepthParams() const noexcept {
        return depth_params;
    }

    SurfaceInterval Interval(SurfaceType type) const noexcept {
        return intervals[Index(type)];
    }

    u32 ResolutionScale() const noexcept {
        return res_scale;
    }

    Common::Rectangle<u32> DrawRect() const noexcept {
        return draw_rect;
    }

    Common::Rectangle<s32> Scissor() const noexcept {
        return scissor_rect;
    }

    ViewportInfo Viewport() const noexcept {
        return viewport;
    }

protected:
    u32 Index(VideoCore::SurfaceType type) const noexcept {
        switch (type) {
        case VideoCore::SurfaceType::Color:
            return 0;
        case VideoCore::SurfaceType::Depth:
        case VideoCore::SurfaceType::DepthStencil:
            return 1;
        default:
            LOG_CRITICAL(HW_GPU, "Unknown surface type in framebuffer");
            return 0;
        }
    }

protected:
    SurfaceParams color_params{};
    SurfaceParams depth_params{};
    std::array<SurfaceInterval, 2> intervals{};
    Common::Rectangle<s32> scissor_rect{};
    Common::Rectangle<u32> draw_rect{};
    ViewportInfo viewport;
    u32 res_scale{1};
};

} // namespace VideoCore
