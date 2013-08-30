/*!
 * Copyright (C) 2013 Akiru Emulator
 *
 * @file    akiry.cpp
 * @author  ShizZy <shizzy247@gmail.com>
 * @date    2012-02-11
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
#include "platform.h"

#if EMU_PLATFORM == PLATFORM_LINUX
#include <unistd.h>
#endif

#include "config.h"
#include "xml.h"
#include "x86_utils.h"

//#include "core.h"
//#include "dvd/loader.h"
//#include "powerpc/cpu_core.h"
//#include "hw/hw.h"
//#include "video_core.h"

#include "emuwindow/emuwindow_glfw.h"

#include "akiru.h"

////////////////////////////////////////////////////////////////////////////////////////////////////
// This is needed to fix SDL in certain build environments
#ifdef main
#undef main
#endif

//#define PLAY_FIFO_RECORDING

/// Application entry point
int __cdecl main(int argc, char **argv) {
    u32 tight_loop;

    LOG_NOTICE(TMASTER, APP_NAME " starting...\n");

    char program_dir[MAX_PATH];
    _getcwd(program_dir, MAX_PATH-1);
    size_t cwd_len = strlen(program_dir);
    program_dir[cwd_len] = '/';
    program_dir[cwd_len+1] = '\0';

    common::ConfigManager config_manager;
    config_manager.set_program_dir(program_dir, MAX_PATH);
    config_manager.ReloadConfig(NULL);
    core::SetConfigManager(&config_manager);

    EmuWindow_GLFW* emu_window = new EmuWindow_GLFW;

    if (E_OK != core::Init(emu_window)) {
        LOG_ERROR(TMASTER, "core initialization failed, exiting...");
        core::Kill();
        exit(1);
    }

#ifndef PLAY_FIFO_RECORDING
    // Load a game or die...
    if (E_OK == dvd::LoadBootableFile(common::g_config->default_boot_file())) {
        if (common::g_config->enable_auto_boot()) {
            core::Start();
        } else {
            LOG_ERROR(TMASTER, "Autoboot required in no-GUI mode... Exiting!\n");
        }
    } else {
        LOG_ERROR(TMASTER, "Failed to load a bootable file... Exiting!\n");
        exit(E_ERR);
    }
    // run the game
    while(core::SYS_DIE != core::g_state) {
        if (core::SYS_RUNNING == core::g_state) {
            if(!(cpu->is_on)) {
                cpu->Start(); // Initialize and start CPU.
            } else {
                for(tight_loop = 0; tight_loop < 10000; ++tight_loop) {
                    cpu->execStep();
                }
            }
        } else if (core::SYS_HALTED == core::g_state) {
            core::Stop();
        }
    }
    core::Kill();
#else
    // load fifo log and replay it

    // TODO: Restructure initialization process - Fix Flipper_Open being called from dvd loaders (wtf?)
    Flipper_Open();
    video_core::Start(emu_window);
    core::SetState(core::SYS_RUNNING);

    fifo_player::FPFile file;
    fifo_player::Load("/home/tony/20_frames.gff", file);
    fifo_player::PlayFile(file);

    // TODO: Wait for video core to finish - PlayFile should handle this
    while (1);
#endif
    delete emu_window;

	return E_OK;
}
