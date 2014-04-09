/**
 * Copyright (C) 2013 citra Emulator
 *
 * @file    citra.cpp
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2013-09-04
 * @brief   Main entry point
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

#include "common/common.h"
#include "common/log_manager.h"
#include "common/file_util.h"

#include "core/system.h"
#include "core/core.h"
#include "core/loader.h"

#include "citra/emu_window/emu_window_glfw.h"

#include "citra/citra.h"

#define E_ERR -1

//#define PLAY_FIFO_RECORDING

/// Application entry point
int __cdecl main(int argc, char **argv) {
    //u32 tight_loop;

    printf("citra starting...\n");

	std::string program_dir = File::GetCurrentDir();

	LogManager::Init();

    EmuWindow_GLFW* emu_window = new EmuWindow_GLFW;

	System::Init(emu_window);

    std::string boot_filename = "C:\\Users\\eric\\Desktop\\3ds\\homebrew\\Mandelbrot3DS.elf";
    std::string error_str;
    
    bool res = Loader::LoadFile(boot_filename, &error_str);

    if (!res) {
        ERROR_LOG(BOOT, "Failed to load ROM: %s", error_str.c_str());
    }

    for (;;) {
        Core::SingleStep();
    }

    delete emu_window;

	return 0;
}
