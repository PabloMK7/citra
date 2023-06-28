// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once

#include "common/math_util.h"

namespace Settings {
enum class LayoutOption : u32;
}

namespace Layout {

/// Orientation of the 3DS displays
enum class DisplayOrientation {
    Landscape,        // Default orientation of the 3DS
    Portrait,         // 3DS rotated 90 degrees counter-clockwise
    LandscapeFlipped, // 3DS rotated 180 degrees counter-clockwise
    PortraitFlipped,  // 3DS rotated 270 degrees counter-clockwise
};

/// Describes the horizontal coordinates for the right eye screen when using Cardboard VR
struct CardboardSettings {
    u32 top_screen_right_eye;
    u32 bottom_screen_right_eye;
    s32 user_x_shift;
};

/// Describes the layout of the window framebuffer (size and top/bottom screen positions)
struct FramebufferLayout {
    u32 width;
    u32 height;
    bool top_screen_enabled;
    bool bottom_screen_enabled;
    Common::Rectangle<u32> top_screen;
    Common::Rectangle<u32> bottom_screen;
    bool is_rotated = true;

    bool additional_screen_enabled;
    Common::Rectangle<u32> additional_screen;

    CardboardSettings cardboard;

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
FramebufferLayout DefaultFrameLayout(u32 width, u32 height, bool is_swapped, bool upright);

/**
 * Factory method for constructing a mobile portrait FramebufferLayout
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @param is_swapped if true, the bottom screen will be displayed above the top screen
 * @return Newly created FramebufferLayout object with mobile portrait screen regions initialized
 */
FramebufferLayout MobilePortraitFrameLayout(u32 width, u32 height, bool is_swapped);

/**
 * Factory method for constructing a Frame with the a 4x size Top screen with a 1x size bottom
 * screen on the right
 * This is useful in particular because it matches well with a 1920x1080 resolution monitor
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @param is_swapped if true, the bottom screen will be the large display
 * @param scale_factor Scale factor to use for bottom screen with respect to top screen
 * @param center_vertical When true, centers the top and bottom screens vertically
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout MobileLandscapeFrameLayout(u32 width, u32 height, bool is_swapped,
                                             float scale_factor, bool center_vertical);

/**
 * Factory method for constructing a FramebufferLayout with only the top or bottom screen
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @param is_swapped if true, the bottom screen will be displayed (and the top won't be displayed)
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout SingleFrameLayout(u32 width, u32 height, bool is_swapped, bool upright);

/**
 * Factory method for constructing a Frame with the a 4x size Top screen with a 1x size bottom
 * screen on the right
 * This is useful in particular because it matches well with a 1920x1080 resolution monitor
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @param is_swapped if true, the bottom screen will be the large display
 * @param scale_factor The ratio between the large screen with respect to the smaller screen
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout LargeFrameLayout(u32 width, u32 height, bool is_swapped, bool upright,
                                   float scale_factor);
/**
 * Factory method for constructing a frame with 2.5 times bigger top screen on the right,
 * and 1x top and bottom screen on the left
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @param is_swapped if true, the bottom screen will be the large display
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout HybridScreenLayout(u32 width, u32 height, bool swapped, bool upright);

/**
 * Factory method for constructing a Frame with the Top screen and bottom
 * screen side by side
 * This is useful for devices with small screens, like the GPDWin
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @param is_swapped if true, the bottom screen will be the left display
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout SideFrameLayout(u32 width, u32 height, bool is_swapped, bool upright);

/**
 * Factory method for constructing a Frame with the Top screen and bottom
 * screen on separate windows
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @param is_secondary if true, the bottom screen will be enabled instead of the top screen
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout SeparateWindowsLayout(u32 width, u32 height, bool is_secondary, bool upright);

/**
 * Factory method for constructing a custom FramebufferLayout
 * @param width Window framebuffer width in pixels
 * @param height Window framebuffer height in pixels
 * @return Newly created FramebufferLayout object with default screen regions initialized
 */
FramebufferLayout CustomFrameLayout(u32 width, u32 height, bool is_swapped);

/**
 * Convenience method to get frame layout by resolution scale
 * Read from the current settings to determine which layout to use.
 * @param res_scale resolution scale factor
 */
FramebufferLayout FrameLayoutFromResolutionScale(u32 res_scale, bool is_secondary = false);

/**
 * Convenience method for transforming a frame layout when using Cardboard VR
 * @param layout frame layout to transform
 * @return layout transformed with the user cardboard settings
 */
FramebufferLayout GetCardboardSettings(const FramebufferLayout& layout);

std::pair<unsigned, unsigned> GetMinimumSizeFromLayout(Settings::LayoutOption layout,
                                                       bool upright_screen);

} // namespace Layout
