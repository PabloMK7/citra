// Copyright 2016 Citra Emulator Project
// Licensed under GPLv2 or any later version
// Refer to the license.txt file included.

#include "common/math_util.h"
#include "common/quaternion.h"
#include "core/frontend/emu_window.h"
#include "core/frontend/motion_emu.h"

namespace Motion {

static constexpr int update_millisecond = 100;
static constexpr auto update_duration =
    std::chrono::duration_cast<std::chrono::steady_clock::duration>(
        std::chrono::milliseconds(update_millisecond));

MotionEmu::MotionEmu(EmuWindow& emu_window)
    : motion_emu_thread(&MotionEmu::MotionEmuThread, this, std::ref(emu_window)) {}

MotionEmu::~MotionEmu() {
    if (motion_emu_thread.joinable()) {
        shutdown_event.Set();
        motion_emu_thread.join();
    }
}

void MotionEmu::MotionEmuThread(EmuWindow& emu_window) {
    auto update_time = std::chrono::steady_clock::now();
    Math::Quaternion<float> q = MakeQuaternion(Math::Vec3<float>(), 0);
    Math::Quaternion<float> old_q;

    while (!shutdown_event.WaitUntil(update_time)) {
        update_time += update_duration;
        old_q = q;

        {
            std::lock_guard<std::mutex> guard(tilt_mutex);

            // Find the quaternion describing current 3DS tilting
            q = MakeQuaternion(Math::MakeVec(-tilt_direction.y, 0.0f, tilt_direction.x),
                               tilt_angle);
        }

        auto inv_q = q.Inverse();

        // Set the gravity vector in world space
        auto gravity = Math::MakeVec(0.0f, -1.0f, 0.0f);

        // Find the angular rate vector in world space
        auto angular_rate = ((q - old_q) * inv_q).xyz * 2;
        angular_rate *= 1000 / update_millisecond / MathUtil::PI * 180;

        // Transform the two vectors from world space to 3DS space
        gravity = QuaternionRotate(inv_q, gravity);
        angular_rate = QuaternionRotate(inv_q, angular_rate);

        // Update the sensor state
        emu_window.AccelerometerChanged(gravity.x, gravity.y, gravity.z);
        emu_window.GyroscopeChanged(angular_rate.x, angular_rate.y, angular_rate.z);
    }
}

void MotionEmu::BeginTilt(int x, int y) {
    mouse_origin = Math::MakeVec(x, y);
    is_tilting = true;
}

void MotionEmu::Tilt(int x, int y) {
    constexpr float SENSITIVITY = 0.01f;
    auto mouse_move = Math::MakeVec(x, y) - mouse_origin;
    if (is_tilting) {
        std::lock_guard<std::mutex> guard(tilt_mutex);
        if (mouse_move.x == 0 && mouse_move.y == 0) {
            tilt_angle = 0;
        } else {
            tilt_direction = mouse_move.Cast<float>();
            tilt_angle = MathUtil::Clamp(tilt_direction.Normalize() * SENSITIVITY, 0.0f,
                                         MathUtil::PI * 0.5f);
        }
    }
}

void MotionEmu::EndTilt() {
    std::lock_guard<std::mutex> guard(tilt_mutex);
    tilt_angle = 0;
    is_tilting = false;
}

} // namespace Motion
