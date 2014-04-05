/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    hw_lcd.cpp
 * @author  bunnei
 * @date    2014-04-05
 * @brief   Hardware LCD interface
 *
 * @section LICENSE
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU
 * General Public License for more details at
 * http://www.gnu.org/copyleft/gpl.html
 *
 * Official project repository can be found at:
 * http://code.google.com/p/gekko-gc-emu/
 */

#include "log.h"
#include "core.h"
#include "hw_lcd.h"

namespace LCD {

static const u32 kFrameTicks = 268123480 / 60;  ///< 268MHz / 60 frames per second

u64 g_last_ticks = 0; ///< Last CPU ticks

template <typename T>
inline void Read(T &var, const u32 addr) {
}

template <typename T>
inline void Write(u32 addr, const T data) {
}

/// Update hardware
void Update() {
    u64 current_ticks = Core::g_app_core->GetTicks();

    if ((current_ticks - g_last_ticks) >= kFrameTicks) {
        g_last_ticks = current_ticks;
        NOTICE_LOG(LCD, "Update frame");
    }
}

/// Initialize hardware
void Init() {
    g_last_ticks = Core::g_app_core->GetTicks();

    NOTICE_LOG(LCD, "LCD initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    NOTICE_LOG(LCD, "LCD shutdown OK");
}

} // namespace
