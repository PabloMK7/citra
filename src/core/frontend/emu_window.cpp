// Copyright 2014 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include <algorithm>
#include <cmath>
#include "common/assert.h"
#include "core/3ds.h"
#include "core/core.h"
#include "core/frontend/emu_window.h"
#include "core/settings.h"

/**
 * Check if the given x/y coordinates are within the touchpad specified by the framebuffer layout
 * @param layout FramebufferLayout object describing the framebuffer size and screen positions
 * @param framebuffer_x Framebuffer x-coordinate to check
 * @param framebuffer_y Framebuffer y-coordinate to check
 * @return True if the coordinates are within the touchpad, otherwise false
 */
static bool IsWithinTouchscreen(const Layout::FramebufferLayout& layout, unsigned framebuffer_x,
                                unsigned framebuffer_y) {
    return (
        framebuffer_y >= layout.bottom_screen.top && framebuffer_y < layout.bottom_screen.bottom &&
        framebuffer_x >= layout.bottom_screen.left && framebuffer_x < layout.bottom_screen.right);
}

std::tuple<unsigned, unsigned> EmuWindow::ClipToTouchScreen(unsigned new_x, unsigned new_y) {
    new_x = std::max(new_x, framebuffer_layout.bottom_screen.left);
    new_x = std::min(new_x, framebuffer_layout.bottom_screen.right - 1);

    new_y = std::max(new_y, framebuffer_layout.bottom_screen.top);
    new_y = std::min(new_y, framebuffer_layout.bottom_screen.bottom - 1);

    return std::make_tuple(new_x, new_y);
}

void EmuWindow::TouchPressed(unsigned framebuffer_x, unsigned framebuffer_y) {
    if (!IsWithinTouchscreen(framebuffer_layout, framebuffer_x, framebuffer_y))
        return;

    touch_x = Core::kScreenBottomWidth * (framebuffer_x - framebuffer_layout.bottom_screen.left) /
              (framebuffer_layout.bottom_screen.right - framebuffer_layout.bottom_screen.left);
    touch_y = Core::kScreenBottomHeight * (framebuffer_y - framebuffer_layout.bottom_screen.top) /
              (framebuffer_layout.bottom_screen.bottom - framebuffer_layout.bottom_screen.top);

    touch_pressed = true;
}

void EmuWindow::TouchReleased() {
    touch_pressed = false;
    touch_x = 0;
    touch_y = 0;
}

void EmuWindow::TouchMoved(unsigned framebuffer_x, unsigned framebuffer_y) {
    if (!touch_pressed)
        return;

    if (!IsWithinTouchscreen(framebuffer_layout, framebuffer_x, framebuffer_y))
        std::tie(framebuffer_x, framebuffer_y) = ClipToTouchScreen(framebuffer_x, framebuffer_y);

    TouchPressed(framebuffer_x, framebuffer_y);
}

void EmuWindow::AccelerometerChanged(float x, float y, float z) {
    constexpr float coef = 512;

    std::lock_guard<std::mutex> lock(accel_mutex);

    // TODO(wwylele): do a time stretch as it in GyroscopeChanged
    // The time stretch formula should be like
    // stretched_vector = (raw_vector - gravity) * stretch_ratio + gravity
    accel_x = static_cast<s16>(x * coef);
    accel_y = static_cast<s16>(y * coef);
    accel_z = static_cast<s16>(z * coef);
}

void EmuWindow::GyroscopeChanged(float x, float y, float z) {
    constexpr float FULL_FPS = 60;
    float coef = GetGyroscopeRawToDpsCoefficient();
    float stretch = Core::System::GetInstance().perf_stats.GetLastFrameTimeScale();
    std::lock_guard<std::mutex> lock(gyro_mutex);
    gyro_x = static_cast<s16>(x * coef * stretch);
    gyro_y = static_cast<s16>(y * coef * stretch);
    gyro_z = static_cast<s16>(z * coef * stretch);
}

void EmuWindow::UpdateCurrentFramebufferLayout(unsigned width, unsigned height) {
    Layout::FramebufferLayout layout;
    if (Settings::values.custom_layout == true) {
        layout = Layout::CustomFrameLayout(width, height);
    } else {
        switch (Settings::values.layout_option) {
        case Settings::LayoutOption::SingleScreen:
            layout = Layout::SingleFrameLayout(width, height, Settings::values.swap_screen);
            break;
        case Settings::LayoutOption::LargeScreen:
            layout = Layout::LargeFrameLayout(width, height, Settings::values.swap_screen);
            break;
        case Settings::LayoutOption::Default:
        default:
            layout = Layout::DefaultFrameLayout(width, height, Settings::values.swap_screen);
            break;
        }
    }
    NotifyFramebufferLayoutChanged(layout);
}
