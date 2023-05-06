// Copyright 2023 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "video_core/rasterizer_cache/framebuffer_base.h"
#include "video_core/rasterizer_cache/surface_base.h"
#include "video_core/regs.h"

namespace VideoCore {

FramebufferBase::FramebufferBase() = default;

FramebufferBase::FramebufferBase(const Pica::Regs& regs, const SurfaceBase* color, u32 color_level,
                                 const SurfaceBase* depth_stencil, u32 depth_level,
                                 Common::Rectangle<u32> surfaces_rect) {
    res_scale = color ? color->res_scale : (depth_stencil ? depth_stencil->res_scale : 1u);

    // Determine the draw rectangle (render area + scissor)
    const Common::Rectangle viewport_rect = regs.rasterizer.GetViewportRect();
    draw_rect.left =
        std::clamp<s32>(static_cast<s32>(surfaces_rect.left) + viewport_rect.left * res_scale,
                        surfaces_rect.left, surfaces_rect.right);
    draw_rect.top =
        std::clamp<s32>(static_cast<s32>(surfaces_rect.bottom) + viewport_rect.top * res_scale,
                        surfaces_rect.bottom, surfaces_rect.top);
    draw_rect.right =
        std::clamp<s32>(static_cast<s32>(surfaces_rect.left) + viewport_rect.right * res_scale,
                        surfaces_rect.left, surfaces_rect.right);
    draw_rect.bottom =
        std::clamp<s32>(static_cast<s32>(surfaces_rect.bottom) + viewport_rect.bottom * res_scale,
                        surfaces_rect.bottom, surfaces_rect.top);

    // Update viewport
    viewport.x = static_cast<s32>(surfaces_rect.left) + viewport_rect.left * res_scale;
    viewport.y = static_cast<s32>(surfaces_rect.bottom) + viewport_rect.bottom * res_scale;
    viewport.width = static_cast<s32>(viewport_rect.GetWidth() * res_scale);
    viewport.height = static_cast<s32>(viewport_rect.GetHeight() * res_scale);

    // Scissor checks are window-, not viewport-relative, which means that if the cached texture
    // sub-rect changes, the scissor bounds also need to be updated.
    scissor_rect.left =
        static_cast<s32>(surfaces_rect.left + regs.rasterizer.scissor_test.x1 * res_scale);
    scissor_rect.bottom =
        static_cast<s32>(surfaces_rect.bottom + regs.rasterizer.scissor_test.y1 * res_scale);

    // x2, y2 have +1 added to cover the entire pixel area, otherwise you might get cracks when
    // scaling or doing multisampling.
    scissor_rect.right =
        static_cast<s32>(surfaces_rect.left + (regs.rasterizer.scissor_test.x2 + 1) * res_scale);
    scissor_rect.top =
        static_cast<s32>(surfaces_rect.bottom + (regs.rasterizer.scissor_test.y2 + 1) * res_scale);

    // Rendering to mipmaps is something quite rare so log it when it occurs.
    if (color_level != 0) {
        LOG_WARNING(HW_GPU, "Game is rendering to color mipmap {}", color_level);
    }
    if (depth_level != 0) {
        LOG_WARNING(HW_GPU, "Game is rendering to depth mipmap {}", depth_level);
    }

    // Query surface invalidation intervals
    const Common::Rectangle draw_rect_unscaled{draw_rect / res_scale};
    if (color) {
        color_params = *color;
        intervals[0] = color->GetSubRectInterval(draw_rect_unscaled, color_level);
    }
    if (depth_stencil) {
        depth_params = *depth_stencil;
        intervals[1] = depth_stencil->GetSubRectInterval(draw_rect_unscaled, depth_level);
    }
}

} // namespace VideoCore
