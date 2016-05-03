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

    FramebufferLayout res {width, height, true, true, {}, {}};

    float window_aspect_ratio = static_cast<float>(height) / width;
    float emulation_aspect_ratio = static_cast<float>(VideoCore::kScreenTopHeight * 4) /
        (VideoCore::kScreenTopWidth * 4 + VideoCore::kScreenBottomWidth);

    if (window_aspect_ratio > emulation_aspect_ratio) {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        int viewport_height = static_cast<int>(std::round(emulation_aspect_ratio * width));

        res.top_screen.left = 0;
        // Top screen occupies 4 / 5ths of the total width
        res.top_screen.right = static_cast<int>(std::round(width / 5)) * 4;
        res.top_screen.top = (height - viewport_height) / 2;
        res.top_screen.bottom = res.top_screen.top + viewport_height;

        int bottom_height = static_cast<int>((static_cast<float>(VideoCore::kScreenBottomHeight) /
            VideoCore::kScreenBottomWidth) * (width - res.top_screen.right));

        res.bottom_screen.left = res.top_screen.right;
        res.bottom_screen.right = width;
        res.bottom_screen.bottom = res.top_screen.bottom;
        res.bottom_screen.top = res.bottom_screen.bottom - bottom_height;
    } else {
        // Otherwise, apply borders to the left and right sides of the window.
        int viewport_width = static_cast<int>(std::round(height / emulation_aspect_ratio));
        // Break the viewport into fifths and give top 4 of them
        int fifth_width = static_cast<int>(std::round(viewport_width / 5));

        res.top_screen.left = (width - viewport_width) / 2;
        res.top_screen.right = res.top_screen.left + fifth_width * 4;
        res.top_screen.top = 0;
        res.top_screen.bottom = height;

        int bottom_height = static_cast<int>((static_cast<float>(VideoCore::kScreenBottomHeight) /
            VideoCore::kScreenBottomWidth) * (fifth_width));

        res.bottom_screen.left = res.top_screen.right;
        res.bottom_screen.right = width - (width - viewport_width) / 2;
        res.bottom_screen.bottom = res.top_screen.bottom;
        res.bottom_screen.top = res.bottom_screen.bottom - bottom_height;
    }

    return res;
}

static FramebufferLayout LargeFrameLayout_Swapped(unsigned width, unsigned height) {

    ASSERT(width > 0);
    ASSERT(height > 0);

    FramebufferLayout res {width, height, true, true, {}, {}};

    float window_aspect_ratio = static_cast<float>(height) / width;
    float emulation_aspect_ratio = static_cast<float>(VideoCore::kScreenBottomHeight * 4) /
        (VideoCore::kScreenBottomWidth * 4 + VideoCore::kScreenTopWidth);

    if (window_aspect_ratio > emulation_aspect_ratio) {
        // Window is narrower than the emulation content => apply borders to the top and bottom
        int viewport_height = static_cast<int>(std::round(emulation_aspect_ratio * width));

        res.bottom_screen.left = 0;
        // Top screen occupies 4 / 5ths of the total width
        res.bottom_screen.right = static_cast<int>(std::round(width / 5)) * 4;
        res.bottom_screen.top = (height - viewport_height) / 2;
        res.bottom_screen.bottom = res.bottom_screen.top + viewport_height;

        int top_height = static_cast<int>((static_cast<float>(VideoCore::kScreenTopHeight) /
            VideoCore::kScreenTopWidth) * (width - res.bottom_screen.right));

        res.top_screen.left = res.bottom_screen.right;
        res.top_screen.right = width;
        res.top_screen.bottom = res.bottom_screen.bottom;
        res.top_screen.top = res.top_screen.bottom - top_height;
    } else {
        // Otherwise, apply borders to the left and right sides of the window.
        int viewport_width = static_cast<int>(std::round(height / emulation_aspect_ratio));
        // Break the viewport into fifths and give top 4 of them
        int fifth_width = static_cast<int>(std::round(viewport_width / 5));

        res.bottom_screen.left = (width - viewport_width) / 2;
        res.bottom_screen.right = res.bottom_screen.left + fifth_width * 4;
        res.bottom_screen.top = 0;
        res.bottom_screen.bottom = height;

        int top_height = static_cast<int>((static_cast<float>(VideoCore::kScreenTopHeight) /
            VideoCore::kScreenTopWidth) * (fifth_width));

        res.top_screen.left = res.bottom_screen.right;
        res.top_screen.right = width - (width - viewport_width) / 2;
        res.top_screen.bottom = res.bottom_screen.bottom;
        res.top_screen.top = res.top_screen.bottom - top_height;
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