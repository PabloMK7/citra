// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "emu_window.h"
#include "video_core/video_core.h"

void EmuWindow::KeyPressed(KeyMap::HostDeviceKey key) {
    Service::HID::PadState mapped_key = KeyMap::GetPadKey(key);

    Service::HID::PadButtonPress(mapped_key);
}

void EmuWindow::KeyReleased(KeyMap::HostDeviceKey key) {
    Service::HID::PadState mapped_key = KeyMap::GetPadKey(key);

    Service::HID::PadButtonRelease(mapped_key);
}

EmuWindow::FramebufferLayout EmuWindow::FramebufferLayout::DefaultScreenLayout(int width, int height) {
    ASSERT(width > 0);
    ASSERT(height > 0);

    EmuWindow::FramebufferLayout res = { width, height, {}, {} };

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
