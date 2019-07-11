// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>

#include "common/assert.h"
#include "core/3ds.h"
#include "core/frontend/framebuffer_layout.h"
#include "core/settings.h"

namespace Layout {

static const float TOP_SCREEN_ASPECT_RATIO =
    static_cast<float>(Core::kScreenTopHeight) / Core::kScreenTopWidth;
static const float BOT_SCREEN_ASPECT_RATIO =
    static_cast<float>(Core::kScreenBottomHeight) / Core::kScreenBottomWidth;

u32 FramebufferLayout::GetScalingRatio() const {
    return static_cast<u32>(((top_screen.GetWidth() - 1) / Core::kScreenTopWidth) + 1);
}

// Finds the largest size subrectangle contained in window area that is confined to the aspect ratio
template <class T>
static Common::Rectangle<T> maxRectangle(Common::Rectangle<T> window_area,
                                         float screen_aspect_ratio) {
    float scale = std::min(static_cast<float>(window_area.GetWidth()),
                           window_area.GetHeight() / screen_aspect_ratio);
    return Common::Rectangle<T>{0, 0, static_cast<T>(std::round(scale)),
                                static_cast<T>(std::round(scale * screen_aspect_ratio))};
}

FramebufferLayout DefaultFrameLayout(u32 width, u32 height, bool swapped) {
    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res{width, height, true, true, {}, {}};
    // Default layout gives equal screen sizes to the top and bottom screen
    Common::Rectangle<u32> screen_window_area{0, 0, width, height / 2};
    Common::Rectangle<u32> top_screen = maxRectangle(screen_window_area, TOP_SCREEN_ASPECT_RATIO);
    Common::Rectangle<u32> bot_screen = maxRectangle(screen_window_area, BOT_SCREEN_ASPECT_RATIO);

    float window_aspect_ratio = static_cast<float>(height) / width;
    // both screens height are taken into account by multiplying by 2
    float emulation_aspect_ratio = TOP_SCREEN_ASPECT_RATIO * 2;

    if (window_aspect_ratio < emulation_aspect_ratio) {
        // Apply borders to the left and right sides of the window.
        top_screen =
            top_screen.TranslateX((screen_window_area.GetWidth() - top_screen.GetWidth()) / 2);
        bot_screen =
            bot_screen.TranslateX((screen_window_area.GetWidth() - bot_screen.GetWidth()) / 2);
    } else {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        // Recalculate the bottom screen to account for the width difference between top and bottom
        screen_window_area = {0, 0, width, top_screen.GetHeight()};
        bot_screen = maxRectangle(screen_window_area, BOT_SCREEN_ASPECT_RATIO);
        bot_screen = bot_screen.TranslateX((top_screen.GetWidth() - bot_screen.GetWidth()) / 2);
        if (swapped) {
            bot_screen = bot_screen.TranslateY(height / 2 - bot_screen.GetHeight());
        } else {
            top_screen = top_screen.TranslateY(height / 2 - top_screen.GetHeight());
        }
    }
    // Move the top screen to the bottom if we are swapped.
    res.top_screen = swapped ? top_screen.TranslateY(height / 2) : top_screen;
    res.bottom_screen = swapped ? bot_screen : bot_screen.TranslateY(height / 2);
    return res;
}

FramebufferLayout SingleFrameLayout(u32 width, u32 height, bool swapped) {
    ASSERT(width > 0);
    ASSERT(height > 0);
    // The drawing code needs at least somewhat valid values for both screens
    // so just calculate them both even if the other isn't showing.
    FramebufferLayout res{width, height, !swapped, swapped, {}, {}};

    Common::Rectangle<u32> screen_window_area{0, 0, width, height};
    Common::Rectangle<u32> top_screen = maxRectangle(screen_window_area, TOP_SCREEN_ASPECT_RATIO);
    Common::Rectangle<u32> bot_screen = maxRectangle(screen_window_area, BOT_SCREEN_ASPECT_RATIO);

    float window_aspect_ratio = static_cast<float>(height) / width;
    float emulation_aspect_ratio = (swapped) ? BOT_SCREEN_ASPECT_RATIO : TOP_SCREEN_ASPECT_RATIO;

    if (window_aspect_ratio < emulation_aspect_ratio) {
        top_screen =
            top_screen.TranslateX((screen_window_area.GetWidth() - top_screen.GetWidth()) / 2);
        bot_screen =
            bot_screen.TranslateX((screen_window_area.GetWidth() - bot_screen.GetWidth()) / 2);
    } else {
        top_screen = top_screen.TranslateY((height - top_screen.GetHeight()) / 2);
        bot_screen = bot_screen.TranslateY((height - bot_screen.GetHeight()) / 2);
    }
    res.top_screen = top_screen;
    res.bottom_screen = bot_screen;
    return res;
}

FramebufferLayout LargeFrameLayout(u32 width, u32 height, bool swapped) {
    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res{width, height, true, true, {}, {}};
    // Split the window into two parts. Give 4x width to the main screen and 1x width to the small
    // To do that, find the total emulation box and maximize that based on window size
    float window_aspect_ratio = static_cast<float>(height) / width;
    float emulation_aspect_ratio =
        swapped ? Core::kScreenBottomHeight * 4 /
                      (Core::kScreenBottomWidth * 4.0f + Core::kScreenTopWidth)
                : Core::kScreenTopHeight * 4 /
                      (Core::kScreenTopWidth * 4.0f + Core::kScreenBottomWidth);
    float large_screen_aspect_ratio = swapped ? BOT_SCREEN_ASPECT_RATIO : TOP_SCREEN_ASPECT_RATIO;
    float small_screen_aspect_ratio = swapped ? TOP_SCREEN_ASPECT_RATIO : BOT_SCREEN_ASPECT_RATIO;

    Common::Rectangle<u32> screen_window_area{0, 0, width, height};
    Common::Rectangle<u32> total_rect = maxRectangle(screen_window_area, emulation_aspect_ratio);
    Common::Rectangle<u32> large_screen = maxRectangle(total_rect, large_screen_aspect_ratio);
    Common::Rectangle<u32> fourth_size_rect = total_rect.Scale(.25f);
    Common::Rectangle<u32> small_screen = maxRectangle(fourth_size_rect, small_screen_aspect_ratio);

    if (window_aspect_ratio < emulation_aspect_ratio) {
        large_screen =
            large_screen.TranslateX((screen_window_area.GetWidth() - total_rect.GetWidth()) / 2);
    } else {
        large_screen = large_screen.TranslateY((height - total_rect.GetHeight()) / 2);
    }
    // Shift the small screen to the bottom right corner
    small_screen =
        small_screen.TranslateX(large_screen.right)
            .TranslateY(large_screen.GetHeight() + large_screen.top - small_screen.GetHeight());
    res.top_screen = swapped ? small_screen : large_screen;
    res.bottom_screen = swapped ? large_screen : small_screen;
    return res;
}

FramebufferLayout SideFrameLayout(u32 width, u32 height, bool swapped) {
    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res{width, height, true, true, {}, {}};
    // Aspect ratio of both screens side by side
    const float emulation_aspect_ratio = static_cast<float>(Core::kScreenTopHeight) /
                                         (Core::kScreenTopWidth + Core::kScreenBottomWidth);
    float window_aspect_ratio = static_cast<float>(height) / width;
    Common::Rectangle<u32> screen_window_area{0, 0, width, height};
    // Find largest Rectangle that can fit in the window size with the given aspect ratio
    Common::Rectangle<u32> screen_rect = maxRectangle(screen_window_area, emulation_aspect_ratio);
    // Find sizes of top and bottom screen
    Common::Rectangle<u32> top_screen = maxRectangle(screen_rect, TOP_SCREEN_ASPECT_RATIO);
    Common::Rectangle<u32> bot_screen = maxRectangle(screen_rect, BOT_SCREEN_ASPECT_RATIO);

    if (window_aspect_ratio < emulation_aspect_ratio) {
        // Apply borders to the left and right sides of the window.
        u32 shift_horizontal = (screen_window_area.GetWidth() - screen_rect.GetWidth()) / 2;
        top_screen = top_screen.TranslateX(shift_horizontal);
        bot_screen = bot_screen.TranslateX(shift_horizontal);
    } else {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        u32 shift_vertical = (screen_window_area.GetHeight() - screen_rect.GetHeight()) / 2;
        top_screen = top_screen.TranslateY(shift_vertical);
        bot_screen = bot_screen.TranslateY(shift_vertical);
    }
    // Move the top screen to the right if we are swapped.
    res.top_screen = swapped ? top_screen.TranslateX(bot_screen.GetWidth()) : top_screen;
    res.bottom_screen = swapped ? bot_screen : bot_screen.TranslateX(top_screen.GetWidth());
    return res;
}

FramebufferLayout CustomFrameLayout(u32 width, u32 height) {
    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res{width, height, true, true, {}, {}};

    Common::Rectangle<u32> top_screen{
        Settings::values.custom_top_left, Settings::values.custom_top_top,
        Settings::values.custom_top_right, Settings::values.custom_top_bottom};
    Common::Rectangle<u32> bot_screen{
        Settings::values.custom_bottom_left, Settings::values.custom_bottom_top,
        Settings::values.custom_bottom_right, Settings::values.custom_bottom_bottom};

    res.top_screen = top_screen;
    res.bottom_screen = bot_screen;
    return res;
}

FramebufferLayout FrameLayoutFromResolutionScale(u32 res_scale) {
    FramebufferLayout layout;
    if (Settings::values.custom_layout == true) {
        layout = CustomFrameLayout(
            std::max(Settings::values.custom_top_right, Settings::values.custom_bottom_right),
            std::max(Settings::values.custom_top_bottom, Settings::values.custom_bottom_bottom));
    } else {
        int width, height;
        switch (Settings::values.layout_option) {
        case Settings::LayoutOption::SingleScreen:
            if (Settings::values.swap_screen) {
                width = Core::kScreenBottomWidth * res_scale;
                height = Core::kScreenBottomHeight * res_scale;
            } else {
                width = Core::kScreenTopWidth * res_scale;
                height = Core::kScreenTopHeight * res_scale;
            }
            layout = SingleFrameLayout(width, height, Settings::values.swap_screen);
            break;
        case Settings::LayoutOption::LargeScreen:
            if (Settings::values.swap_screen) {
                width = (Core::kScreenBottomWidth + Core::kScreenTopWidth / 4) * res_scale;
                height = Core::kScreenBottomHeight * res_scale;
            } else {
                width = (Core::kScreenTopWidth + Core::kScreenBottomWidth / 4) * res_scale;
                height = Core::kScreenTopHeight * res_scale;
            }
            layout = LargeFrameLayout(width, height, Settings::values.swap_screen);
            break;
        case Settings::LayoutOption::SideScreen:
            width = (Core::kScreenTopWidth + Core::kScreenBottomWidth) * res_scale;
            height = Core::kScreenTopHeight * res_scale;
            layout = SideFrameLayout(width, height, Settings::values.swap_screen);
            break;
        case Settings::LayoutOption::Default:
        default:
            width = Core::kScreenTopWidth * res_scale;
            height = (Core::kScreenTopHeight + Core::kScreenBottomHeight) * res_scale;
            layout = DefaultFrameLayout(width, height, Settings::values.swap_screen);
            break;
        }
    }
    return layout;
}

} // namespace Layout
