/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    hw_lcd.h
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

#pragma once

#include "common_types.h"

namespace LCD {

enum {
    TOP_ASPECT_X        = 0x5,
    TOP_ASPECT_Y        = 0x3,
    
    TOP_HEIGHT          = 240,
    TOP_WIDTH           = 400,
    BOTTOM_WIDTH        = 320,

    FRAMEBUFFER_SEL     = 0x20184E59,
    TOP_LEFT_FRAME1     = 0x20184E60,
    TOP_LEFT_FRAME2     = 0x201CB370,
    TOP_RIGHT_FRAME1    = 0x20282160,
    TOP_RIGHT_FRAME2    = 0x202C8670,
    SUB_FRAME1          = 0x202118E0,
    SUB_FRAME2          = 0x20249CF0,
};

template <typename T>
inline void Read(T &var, const u32 addr);

template <typename T>
inline void Write(u32 addr, const T data);

/// Update hardware
void Update();

/// Initialize hardware
void Init();

/// Shutdown hardware
void Shutdown();


} // namespace
