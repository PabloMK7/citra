// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <cmath>

#include "common/assert.h"
#include "common/framebuffer_layout.h"
#include "video_core/video_core.h"

namespace Layout {
static FramebufferLayout DefaultFrameLayout(unsigned width, unsigned height) {

    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res {width, height, true, true, {}, {}};

    float window_aspect_ratio = static_cast<float>(height) / width;
    float emulation_aspect_ratio = static_cast<float>(VideoCore::kScreenTopHeight * 2) /
        VideoCore::kScreenTopWidth;

    if (window_aspect_ratio > emulation_aspect_ratio) {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        int viewport_height = static_cast<int>(std::round(emulation_aspect_ratio * width));

        res.top_screen.left = 0;
        res.top_screen.right = res.top_screen.left + width;
        res.top_screen.top = (height - viewport_height) / 2;
        res.top_screen.bottom = res.top_screen.top + viewport_height / 2;

        int bottom_width = static_cast<int>((static_cast<float>(VideoCore::kScreenBottomWidth) /
            VideoCore::kScreenTopWidth) * (res.top_screen.right - res.top_screen.left));
        int bottom_border = ((res.top_screen.right - res.top_screen.left) - bottom_width) / 2;

        res.bottom_screen.left = bottom_border;
        res.bottom_screen.right = res.bottom_screen.left + bottom_width;
        res.bottom_screen.top = res.top_screen.bottom;
        res.bottom_screen.bottom = res.bottom_screen.top + viewport_height / 2;
    } else {
        // Otherwise, apply borders to the left and right sides of the window.
        int viewport_width = static_cast<int>(std::round(height / emulation_aspect_ratio));

        res.top_screen.left = (width - viewport_width) / 2;
        res.top_screen.right = res.top_screen.left + viewport_width;
        res.top_screen.top = 0;
        res.top_screen.bottom = res.top_screen.top + height / 2;

        int bottom_width = static_cast<int>((static_cast<float>(VideoCore::kScreenBottomWidth) /
            VideoCore::kScreenTopWidth) * (res.top_screen.right - res.top_screen.left));
        int bottom_border = ((res.top_screen.right - res.top_screen.left) - bottom_width) / 2;

        res.bottom_screen.left = res.top_screen.left + bottom_border;
        res.bottom_screen.right = res.bottom_screen.left + bottom_width;
        res.bottom_screen.top = res.top_screen.bottom;
        res.bottom_screen.bottom = res.bottom_screen.top + height / 2;
    }

    return res;
}

static FramebufferLayout DefaultFrameLayout_Swapped(unsigned width, unsigned height) {

    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res {width, height, true, true, {}, {}};

    float window_aspect_ratio = static_cast<float>(height) / width;
    float emulation_aspect_ratio = static_cast<float>(VideoCore::kScreenTopHeight * 2) /
        VideoCore::kScreenTopWidth;

    if (window_aspect_ratio > emulation_aspect_ratio) {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        int viewport_height = static_cast<int>(std::round(emulation_aspect_ratio * width));

        res.top_screen.left = 0;
        res.top_screen.right = res.top_screen.left + width;

        int bottom_width = static_cast<int>((static_cast<float>(VideoCore::kScreenBottomWidth) /
            VideoCore::kScreenTopWidth) * (res.top_screen.right - res.top_screen.left));
        int bottom_border = ((res.top_screen.right - res.top_screen.left) - bottom_width) / 2;

        res.bottom_screen.left = bottom_border;
        res.bottom_screen.right = res.bottom_screen.left + bottom_width;
        res.bottom_screen.top = (height - viewport_height) / 2;
        res.bottom_screen.bottom = res.bottom_screen.top + viewport_height / 2;

        res.top_screen.top = res.bottom_screen.bottom;
        res.top_screen.bottom = res.top_screen.top + viewport_height / 2;
    } else {
        // Otherwise, apply borders to the left and right sides of the window.
        int viewport_width = static_cast<int>(std::round(height / emulation_aspect_ratio));
        res.top_screen.left = (width - viewport_width) / 2;
        res.top_screen.right = res.top_screen.left + viewport_width;

        int bottom_width = static_cast<int>((static_cast<float>(VideoCore::kScreenBottomWidth) /
            VideoCore::kScreenTopWidth) * (res.top_screen.right - res.top_screen.left));
        int bottom_border = ((res.top_screen.right - res.top_screen.left) - bottom_width) / 2;

        res.bottom_screen.left = res.top_screen.left + bottom_border;
        res.bottom_screen.right = res.bottom_screen.left + bottom_width;
        res.bottom_screen.top = 0;
        res.bottom_screen.bottom = res.bottom_screen.top + height / 2;

        res.top_screen.top = res.bottom_screen.bottom;
        res.top_screen.bottom = res.top_screen.top + height / 2;
    }

    return res;
}

static FramebufferLayout SingleFrameLayout(unsigned width, unsigned height) {

    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res {width, height, true, false, {}, {}};

    float window_aspect_ratio = static_cast<float>(height) / width;
    float emulation_aspect_ratio = static_cast<float>(VideoCore::kScreenTopHeight) /
        VideoCore::kScreenTopWidth;

    if (window_aspect_ratio > emulation_aspect_ratio) {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        int viewport_height = static_cast<int>(std::round(emulation_aspect_ratio * width));

        res.top_screen.left = 0;
        res.top_screen.right = res.top_screen.left + width;
        res.top_screen.top = (height - viewport_height) / 2;
        res.top_screen.bottom = res.top_screen.top + viewport_height;

        res.bottom_screen.left = 0;
        res.bottom_screen.right = VideoCore::kScreenBottomWidth;
        res.bottom_screen.top = 0;
        res.bottom_screen.bottom = VideoCore::kScreenBottomHeight;
    } else {
        // Otherwise, apply borders to the left and right sides of the window.
        int viewport_width = static_cast<int>(std::round(height / emulation_aspect_ratio));

        res.top_screen.left = (width - viewport_width) / 2;
        res.top_screen.right = res.top_screen.left + viewport_width;
        res.top_screen.top = 0;
        res.top_screen.bottom = res.top_screen.top + height;

        // The Rasterizer still depends on these fields to maintain the right aspect ratio
        res.bottom_screen.left = 0;
        res.bottom_screen.right = VideoCore::kScreenBottomWidth;
        res.bottom_screen.top = 0;
        res.bottom_screen.bottom = VideoCore::kScreenBottomHeight;
    }

    return res;
}

static FramebufferLayout SingleFrameLayout_Swapped(unsigned width, unsigned height) {

    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res {width, height, false, true, {}, {}};

    float window_aspect_ratio = static_cast<float>(height) / width;
    float emulation_aspect_ratio = static_cast<float>(VideoCore::kScreenBottomHeight) /
        VideoCore::kScreenBottomWidth;

    if (window_aspect_ratio > emulation_aspect_ratio) {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        int viewport_height = static_cast<int>(std::round(emulation_aspect_ratio * width));

        res.bottom_screen.left = 0;
        res.bottom_screen.right = res.bottom_screen.left + width;
        res.bottom_screen.top = (height - viewport_height) / 2;
        res.bottom_screen.bottom = res.bottom_screen.top + viewport_height;

        // The Rasterizer still depends on these fields to maintain the right aspect ratio
        res.top_screen.left = 0;
        res.top_screen.right = VideoCore::kScreenTopWidth;
        res.top_screen.top = 0;
        res.top_screen.bottom = VideoCore::kScreenTopHeight;
    } else {
        // Otherwise, apply borders to the left and right sides of the window.
        int viewport_width = static_cast<int>(std::round(height / emulation_aspect_ratio));

        res.bottom_screen.left = (width - viewport_width) / 2;
        res.bottom_screen.right = res.bottom_screen.left + viewport_width;
        res.bottom_screen.top = 0;
        res.bottom_screen.bottom = res.bottom_screen.top + height;

        res.top_screen.left = 0;
        res.top_screen.right = VideoCore::kScreenTopWidth;
        res.top_screen.top = 0;
        res.top_screen.bottom = VideoCore::kScreenTopHeight;
    }

    return res;
}

static FramebufferLayout LargeFrameLayout(unsigned width, unsigned height) {

    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res{ width, height, true, true,{},{} };

    float window_aspect_ratio = static_cast<float>(width) / height;
    float top_screen_aspect_ratio = static_cast<float>(VideoCore::kScreenTopWidth) /
        VideoCore::kScreenTopHeight;

    int viewport_height = static_cast<int>(std::round((width - VideoCore::kScreenBottomWidth) /
        top_screen_aspect_ratio));
    int viewport_width = static_cast<int>(std::round((height * top_screen_aspect_ratio) +
        VideoCore::kScreenBottomWidth));
    float emulation_aspect_ratio = static_cast<float>(width) / viewport_height;

    if (window_aspect_ratio < emulation_aspect_ratio) {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        res.top_screen.left = 0;
        res.top_screen.right = width - VideoCore::kScreenBottomWidth;
        res.top_screen.top = (height - viewport_height) / 2;
        res.top_screen.bottom = viewport_height + res.top_screen.top;

        res.bottom_screen.left = res.top_screen.right;
        res.bottom_screen.right = width;
        res.bottom_screen.bottom = res.top_screen.bottom;
        res.bottom_screen.top = res.bottom_screen.bottom - VideoCore::kScreenBottomHeight;
    } else {
        // Otherwise, apply borders to the left and right sides of the window.
        res.top_screen.left = (width - viewport_width) / 2;
        res.top_screen.right = (top_screen_aspect_ratio * height) + res.top_screen.left;
        res.top_screen.top = 0;
        res.top_screen.bottom = height;

        res.bottom_screen.left = res.top_screen.right;
        res.bottom_screen.right = res.bottom_screen.left + VideoCore::kScreenBottomWidth;
        res.bottom_screen.bottom = height;
        res.bottom_screen.top = height - VideoCore::kScreenBottomHeight;
    }

    return res;
}

static FramebufferLayout LargeFrameLayout_Swapped(unsigned width, unsigned height) {

    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res {width, height, true, true, {}, {}};

    float window_aspect_ratio = static_cast<float>(width) / height;
    float bottom_screen_aspect_ratio = static_cast<float>(VideoCore::kScreenBottomWidth) /
        VideoCore::kScreenBottomHeight;

    int viewport_height = static_cast<int>(std::round((width - VideoCore::kScreenTopWidth) /
        bottom_screen_aspect_ratio));
    int viewport_width = static_cast<int>(std::round((height * bottom_screen_aspect_ratio) +
        VideoCore::kScreenTopWidth));
    float emulation_aspect_ratio = static_cast<float>(width) / viewport_height;

    if (window_aspect_ratio < emulation_aspect_ratio) {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        res.bottom_screen.left = 0;
        res.bottom_screen.right = width - VideoCore::kScreenTopWidth;
        res.bottom_screen.top = (height - viewport_height) / 2;
        res.bottom_screen.bottom = viewport_height + res.bottom_screen.top;

        res.top_screen.left = res.bottom_screen.right;
        res.top_screen.right = width;
        res.top_screen.bottom = res.bottom_screen.bottom;
        res.top_screen.top = res.top_screen.bottom - VideoCore::kScreenTopHeight;
    } else {
        // Otherwise, apply borders to the left and right sides of the window.
        res.bottom_screen.left = (width - viewport_width) / 2;
        res.bottom_screen.right = (bottom_screen_aspect_ratio * height) + res.bottom_screen.left;
        res.bottom_screen.top = 0;
        res.bottom_screen.bottom = height;

        res.top_screen.left = res.bottom_screen.right;
        res.top_screen.right = res.top_screen.left + VideoCore::kScreenTopWidth;
        res.top_screen.bottom = height;
        res.top_screen.top = height - VideoCore::kScreenTopHeight;
    }

    return res;
}

FramebufferLayout DefaultFrameLayout(unsigned width, unsigned height, bool is_swapped) {
    return is_swapped ? DefaultFrameLayout_Swapped(width, height) : DefaultFrameLayout(width, height);
}

FramebufferLayout SingleFrameLayout(unsigned width, unsigned height, bool is_swapped) {
    return is_swapped ? SingleFrameLayout_Swapped(width, height) : SingleFrameLayout(width, height);
}

FramebufferLayout LargeFrameLayout(unsigned width, unsigned height, bool is_swapped) {
    return is_swapped ? LargeFrameLayout_Swapped(width, height) : LargeFrameLayout(width, height);
}
}
