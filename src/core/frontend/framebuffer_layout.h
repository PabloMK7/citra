// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/math_util.h"

namespace Layout {

/// Describes the layout of the window framebuffer (size and top/bottom screen positions)
struct FramebufferLayout {
    u32 width;
    u32 height;
    bool top_screen_enabled;
    bool bottom_screen_enabled;
    Common::Rectangle<u32> top_screen;
    Common::Rectangle<u32> bottom_screen;

    /**
     * Returns the ration of pixel size of the top screen, compared to the native size of the 3DS
     * screen.
     */
    u32 GetScalingRatio() const;
};

/**
 * Factory method for constructing a default FramebufferLayout
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @param is_swapped if true, the bottom screen will be displayed above the top screen
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout DefaultFrameLayout(u32 width, u32 height, bool is_swapped);

/**
 * Factory method for constructing a FramebufferLayout with only the top or bottom screen
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @param is_swapped if true, the bottom screen will be displayed (and the top won't be displayed)
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout SingleFrameLayout(u32 width, u32 height, bool is_swapped);

/**
 * Factory method for constructing a Frame with the a 4x size Top screen with a 1x size bottom
 * screen on the right
 * This is useful in particular because it matches well with a 1920x1080 resolution monitor
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @param is_swapped if true, the bottom screen will be the large display
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout LargeFrameLayout(u32 width, u32 height, bool is_swapped);

/**
 * Factory method for constructing a Frame with the Top screen and bottom
 * screen side by side
 * This is useful for devices with small screens, like the GPDWin
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @param is_swapped if true, the bottom screen will be the left display
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout SideFrameLayout(u32 width, u32 height, bool is_swapped);

/**
 * Factory method for constructing a custom FramebufferLayout
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout CustomFrameLayout(u32 width, u32 height);

/**
 * Convenience method to get frame layout by resolution scale
 * Read from the current settings to determine which layout to use.
 * @param res_scale resolution scale factor
 */
FramebufferLayout FrameLayoutFromResolutionScale(u32 res_scale);

} // namespace Layout
