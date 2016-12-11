// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#pragma once
#include "common/thread.h"
#include "common/vector_math.h"

class EmuWindow;

namespace Motion {

class MotionEmu final {
public:
    MotionEmu(EmuWindow& emu_window);
    ~MotionEmu();

    /**
     * Signals that a motion sensor tilt has begun.
     * @param x the x-coordinate of the cursor
     * @param y the y-coordinate of the cursor
     */
    void BeginTilt(int x, int y);

    /**
     * Signals that a motion sensor tilt is occurring.
     * @param x the x-coordinate of the cursor
     * @param y the y-coordinate of the cursor
     */
    void Tilt(int x, int y);

    /**
     * Signals that a motion sensor tilt has ended.
     */
    void EndTilt();

private:
    Math::Vec2<int> mouse_origin;

    std::mutex tilt_mutex;
    Math::Vec2<float> tilt_direction;
    float tilt_angle = 0;

    bool is_tilting = false;

    Common::Event shutdown_event;
    std::thread motion_emu_thread;

    void MotionEmuThread(EmuWindow& emu_window);
};

} // namespace Motion
