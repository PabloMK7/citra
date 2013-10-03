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

#include "common.h"
#include "log_manager.h"
#include "file_util.h"

#include "system.h"

#include "emu_window/emu_window_glfw.h"

#include "citra.h"

//#define PLAY_FIFO_RECORDING

/// Application entry point
int __cdecl main(int argc, char **argv) {
    //u32 tight_loop;

    printf("citra starting...\n");

	std::string program_dir = File::GetCurrentDir();

	LogManager::Init();

    EmuWindow_GLFW* emu_window = new EmuWindow_GLFW;

	System::Init(emu_window);

    //if (E_OK != core::Init(emu_window)) {
    //    LOG_ERROR(TMASTER, "core initialization failed, exiting...");
    //    core::Kill();
    //    exit(1);
    //}

    //// Load a game or die...
    //if (E_OK == dvd::LoadBootableFile(common::g_config->default_boot_file())) {
    //    if (common::g_config->enable_auto_boot()) {
    //        core::Start();
    //    } else {
    //        LOG_ERROR(TMASTER, "Autoboot required in no-GUI mode... Exiting!\n");
    //    }
    //} else {
    //    LOG_ERROR(TMASTER, "Failed to load a bootable file... Exiting!\n");
    //    exit(E_ERR);
    //}
    //// run the game
    //while(core::SYS_DIE != core::g_state) {
    //    if (core::SYS_RUNNING == core::g_state) {
    //        if(!(cpu->is_on)) {
    //            cpu->Start(); // Initialize and start CPU.
    //        } else {
    //            for(tight_loop = 0; tight_loop < 10000; ++tight_loop) {
    //                cpu->execStep();
    //            }
    //        }
    //    } else if (core::SYS_HALTED == core::g_state) {
    //        core::Stop();
    //    }
    //}
    //core::Kill();

	while (1) {
	}

    //delete emu_window;

	return 0;
}
