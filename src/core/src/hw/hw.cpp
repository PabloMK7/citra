/**
 * Copyright (C) 2013 Citrus Emulator
 *
 * @file    hw.cpp
 * @author  bunnei
 * @date    2014-04-04
 * @brief   Hardware interface
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
#include "hw/hw.h"
#include "hw/hw_lcd.h"

namespace HW {

template <typename T>
inline void Read(T &var, const u32 addr) {
    NOTICE_LOG(HW, "Hardware read from address %08X", addr);
}

template <typename T>
inline void Write(u32 addr, const T data) {
    NOTICE_LOG(HW, "Hardware write to address %08X", addr);
}

// Explicitly instantiate template functions because we aren't defining this in the header:

template void Read<u64>(u64 &var, const u32 addr);
template void Read<u32>(u32 &var, const u32 addr);
template void Read<u16>(u16 &var, const u32 addr);
template void Read<u8>(u8 &var, const u32 addr);

template void Write<const u64>(u32 addr, const u64 data);
template void Write<const u32>(u32 addr, const u32 data);
template void Write<const u16>(u32 addr, const u16 data);
template void Write<const u8>(u32 addr, const u8 data);

/// Update hardware
void Update() {
    LCD::Update();
}

/// Initialize hardware
void Init() {
    LCD::Init();
    NOTICE_LOG(HW, "Hardware initialized OK");
}

/// Shutdown hardware
void Shutdown() {
    NOTICE_LOG(HW, "Hardware shutdown OK");
}

}