/**
* Copyright (C) 2013 Citrus Emulator
*
* @file    system.cpp
* @author  ShizZy <shizzy247@gmail.com>
* @date    2013-09-26
* @brief   Emulation of main system
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

#include "core.h"
#include "hw/hw.h"
#include "core_timing.h"
#include "mem_map.h"
#include "system.h"
#include "video_core.h"

namespace System {

volatile State g_state;
MetaFileSystem g_ctr_file_system;

void UpdateState(State state) {
}

void Init(EmuWindow* emu_window) {
	Core::Init();
	Memory::Init();
    HW::Init();
	CoreTiming::Init();
    VideoCore::Init(emu_window);
}

void RunLoopFor(int cycles) {
	RunLoopUntil(CoreTiming::GetTicks() + cycles);
}

void RunLoopUntil(u64 global_cycles) {
}

void Shutdown() {
    Core::Shutdown();
    HW::Shutdown();
    VideoCore::Shutdown();
	g_ctr_file_system.Shutdown();
}

} // namespace
